package main

import (
	"fmt"
	"time"
)

// adjustVolume changes system volume and shows overlay
func (app *MiyooPod) adjustVolume(delta int) {
        // Volume is owned by SpruceOS kernel - just read ALSA and show overlay
        audioSetVolume(100) // Keep SDL_mixer internal level maxed
        if vol := getAlsaVolume(); vol >= 0 {
                app.SystemVolume = vol
        }
        app.showOverlay("volume", app.SystemVolume)
}
func (app *MiyooPod) adjustBrightness(delta int) {
	newBrightness := clamp(app.SystemBrightness+delta, 10, 100) // Min 10% so screen stays visible

	setBrightness(newBrightness)
	app.SystemBrightness = newBrightness

	app.showOverlay("brightness", newBrightness)

	// Persist to settings
	go app.saveSettings()

	logMsg(fmt.Sprintf("Brightness: %d%%", newBrightness))
}

// showOverlay displays the volume/brightness overlay for 2 seconds
func (app *MiyooPod) showOverlay(overlayType string, value int) {
	// Cancel existing timer
	if app.OverlayTimer != nil {
		app.OverlayTimer.Stop()
	}

	app.OverlayType = overlayType
	app.OverlayValue = value
	app.OverlayVisible = true

	// Signal main loop to redraw (non-blocking to avoid deadlock)
	app.requestRedraw()

	// Hide after 2 seconds
	app.OverlayTimer = time.AfterFunc(2*time.Second, func() {
		app.OverlayVisible = false
		app.requestRedraw()
	})
}

func clamp(value, min, max int) int {
	if value < min {
		return min
	}
	if value > max {
		return max
	}
	return value
}
