PSBox Cloud Gaming for Windows - Portable Edition
=================================================

Xbox Cloud Gaming (xCloud) native client: browse the cloud catalog and stream
games with sound, with a controller (Xbox, DualSense, DualShock 4, 8BitDo,
Switch Pro...), keyboard and mouse.

You need a Microsoft account and a Game Pass plan with cloud gaming
(free-to-play games like Fortnite stream without it once on your account).
Unofficial project, not affiliated with, endorsed or sponsored by
Microsoft or Sony.


Running it
----------
Extract the zip archive to any folder and run:

    psbox.exe

Optional command-line parameters:
    --windowed      Start in windowed mode
    --fullscreen    Start in fullscreen mode
    --data-dir <dir> Store user data in custom folder

F11 or Alt+Enter toggles fullscreen; Settings > Display mode remembers it.

Self-test without signing in:
    psbox.exe --selftest C:\Temp\psbox-selftest


Your data
---------
Sign-in credentials, settings, caches and log (xcloud.log) are kept in:
    %LOCALAPPDATA%\psbox\ (or specified by --data-dir)
Nothing is written to the program folder. Deleting %LOCALAPPDATA%\psbox\
completely resets the app.


What it needs
-------------
Windows 10 or 11, 64-bit. Keep psbox.exe, SDL3.dll and the assets folder
together. The first start may show a Windows Firewall question: the app's
remote web control listens on your home network (port 8080); "Cancel" is fine
if you don't use it. Windows SmartScreen may warn about an unsigned app:
"More info" > "Run anyway".

Video is decoded by the GPU (Direct3D 11) when it can, else by the CPU;
Settings > Video decoder chooses. The sharpening / upscaling filters need a
GPU driver with Vulkan 1.3; without it the picture is drawn plainly.

Updates
-------
The app looks for a new version each time it starts and, when there is one,
installs it straight away and starts again as the new version (a game that is
starting first: then it asks). Your sign-in and settings are kept: they live
in %LOCALAPPDATA%\psbox, not in this folder. Keep the folder somewhere you can
write to (Desktop, Documents, a games folder), not under Program Files.
An update is only installed when it carries the right signature, and the old
files go back if one is cut short.

Xbox One mode (experimental)
----------------------------
"PSBox as Xbox One.bat" starts the app telling xCloud it is an Xbox One
console instead of a PC (the same as psbox.exe --as-xbox-one, or setting
PSBOX_DEVICE=xboxone). Some games are offered to consoles but not to PCs or
browsers; this may make them show up. Games refused with "DeviceTypeNotAllowed"
stay refused: that check goes by how you signed in, not by this. The stream is 1080p in this
mode whatever Settings > Stream resolution says. If games stop starting
("session failed"), go back to psbox.exe: xCloud may not accept the
description. The log (below) says "starting as an Xbox One console" for
each game started in this mode.

If something goes wrong, the log is %LOCALAPPDATA%\psbox\xcloud.log.


Controllers
-----------
Standard XInput controllers (Xbox One, Xbox Series, 8BitDo) work out of the box.
PlayStation DualSense and DualShock 4 controllers are supported via SDL3 USB/Bluetooth HID.
