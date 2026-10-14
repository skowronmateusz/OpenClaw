OpenClaw - Captain Claw (1997) / Kapitan Pazur for macOS (Apple Silicon)
=======================================================================

OpenClaw is a reimplementation of the Captain Claw platformer. You need your
own copy of the original game; the game data is not included. Both the
English release (Captain Claw) and the Polish release (Kapitan Pazur) are
supported, the language is detected from CLAW.REZ.

Installation
------------

1. Drag OpenClaw.app into the Applications folder.

2. Start OpenClaw. The app is not notarized by Apple, so the first time macOS
   blocks it: right-click the app and choose Open, or allow it in
   System Settings > Privacy & Security. If macOS says the app is damaged,
   run in Terminal:

       xattr -dr com.apple.quarantine /Applications/OpenClaw.app

3. On the first start OpenClaw opens the folder

       ~/Library/Application Support/OpenClaw/

   Copy these files from your game disc (or a mounted ISO image) into it:

       DATA/CLAW.REZ                         -> CLAW.REZ   (required)
       MUSIC/CLAW4.SF2, MUSIC/CLAW.SF2       -> same names (MIDI music)
       MOVIES/claw.fec                       -> CLAW.FEC   (movies)

   For example in Terminal (adjust the name of the mounted disc):

       cd ~/Library/Application\ Support/OpenClaw
       cp "/Volumes/KAPITAN PAZUR/DATA/CLAW.REZ" .
       cp "/Volumes/KAPITAN PAZUR/MUSIC/"*.SF2 .
       cp "/Volumes/KAPITAN PAZUR/MOVIES/claw.fec" CLAW.FEC

4. Start OpenClaw again.

Settings (config.xml) and saved games (SAVES.XML) are stored in the same
folder. Delete them to restore the defaults.

Project page: https://github.com/skowronmateusz/OpenClaw-PL-macos
