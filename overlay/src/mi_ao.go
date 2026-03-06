package main

import (
	"fmt"
	"os/exec"
	"strings"
)

// getAlsaVolume reads current Soft Volume Master level (0-100)
func getAlsaVolume() int {
        out, err := exec.Command("amixer", "sget", "Soft Volume Master").Output()
        if err != nil {
                return -1
        }
        s := string(out)
        pct := 0
        idx := strings.Index(s, "Front Left:")
        if idx >= 0 {
                fmt.Sscanf(s[idx:], "Front Left: %*d [%d%%]", &pct)
        }
        return pct
}

