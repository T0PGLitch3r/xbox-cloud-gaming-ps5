@echo off
rem PSBox as an Xbox One console: xCloud is told this is an Xbox One, for the
rem games it offers to consoles only. Experimental: if a game will not start,
rem use psbox.exe as usual.
start "" "%~dp0psbox.exe" --as-xbox-one %*
