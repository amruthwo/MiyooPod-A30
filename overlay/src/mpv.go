package main

import (
	"time"
)

// startPlaybackPoller checks audio state and updates progress display.
// Runs in its own goroutine. Minimal work per tick to avoid starving audio.
func (app *MiyooPod) startPlaybackPoller() {
	lastDrawnSecond := -1
	tickCount := 0
	saveTickCount := 0
	lastWallTime := time.Now()
	lastPosition := 0.0

	for app.Running {
		if app.Playing != nil && app.Playing.State != StateStopped {
			state := audioGetState()

			if state.Position >= 0 && !app.SeekActive {
				app.Playing.Position = state.Position
			}
			// Detect post-sleep drift: position advancing faster than wall time
			now := time.Now()
			wallElapsed := now.Sub(lastWallTime).Seconds()
			posElapsed := state.Position - lastPosition
			if wallElapsed > 0.5 && posElapsed > wallElapsed*1.5 && state.IsPlaying {
				logMsg("INFO: Audio drift detected (post-sleep), reinitializing audio")
				audioReinit()
				audioSetVolume(100)
			}
			lastWallTime = now
			lastPosition = state.Position
			if state.Duration > 0 && app.Playing.Track != nil && app.Playing.Track.Duration == 0 {
				app.Playing.Track.Duration = state.Duration
			}

			if state.IsPaused && app.Playing.State != StatePaused {
				app.Playing.State = StatePaused
				app.NPCacheDirty = true
				app.requestRedraw()
			} else if state.IsPlaying && app.Playing.State != StatePlaying {
				app.Playing.State = StatePlaying
				app.NPCacheDirty = true
				app.requestRedraw()
			}

			if state.Finished {
				app.handleTrackEnd()
			}

			// Update progress bar when on Now Playing screen and second changes
			if app.CurrentScreen == ScreenNowPlaying {
				currentSecond := int(app.Playing.Position)
				if currentSecond != lastDrawnSecond {
					lastDrawnSecond = currentSecond
					app.updateProgressBarOnly()
				}
			}

			// Redraw lyrics screen only when the highlighted LRC line changes,
			// and only when the user is not holding a scroll key (avoids flash during scroll).
			if app.CurrentScreen == ScreenLyrics && app.LyricsCachedLRC != nil && app.LastKey == NONE {
				activeLRC := activeLRCIndex(app.LyricsCachedLRC, app.Playing.Position)
				if activeLRC != app.LyricsLastActiveLRC {
					app.LyricsLastActiveLRC = activeLRC
					app.requestRedraw()
				}
			}

			// Flush audio buffers every 5 seconds to prevent choppy playback
			// Mimics the fix that happens when user manually pauses/resumes
			tickCount++
			if tickCount >= 5 {
				audioFlushBuffers()
				tickCount = 0
			}

			// Save playback state every 3 seconds during active playback
			saveTickCount++
			if saveTickCount >= 3 {
				app.savePlaybackState()
				saveTickCount = 0
			}
		}
		// Increased sleep to reduce CPU usage and SD card contention
		time.Sleep(1000 * time.Millisecond)
	}
}

func (app *MiyooPod) mpvLoadFile(path string) error {
	// Stream from SD card with larger buffer (128KB) to reduce underruns
	err := audioLoadFile(path)
	if err != nil {
		return err
	}
	err = audioPlay()
	if err != nil {
		return err
	}
	return nil
}

func (app *MiyooPod) mpvTogglePause() {
	audioTogglePause()
}

func (app *MiyooPod) mpvStop() {
	audioStop()
}

func (app *MiyooPod) mpvSeekAbsolute(position float64) {
	if app.Playing == nil {
		return
	}
	if position < 0 {
		position = 0
	}
	if position > app.Playing.Duration && app.Playing.Duration > 0 {
		position = app.Playing.Duration
	}
	audioSeek(position)
}

func (app *MiyooPod) mpvSeek(seconds float64) {
	if app.Playing == nil {
		return
	}
	newPos := app.Playing.Position + seconds
	if newPos < 0 {
		newPos = 0
	}
	if newPos > app.Playing.Duration && app.Playing.Duration > 0 {
		newPos = app.Playing.Duration
	}
	audioSeek(newPos)
}
