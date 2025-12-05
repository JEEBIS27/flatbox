This folder contains CircuitPython code acting as a game controller that can be used on Flatbox rev4, rev5, rev8 and rev9.

To use it, first you have to flash CircuitPython onto your board. The [binaries](binaries) folder contains unofficial CircuitPython builds[^1] for the Flatbox revisions mentioned above. The process of flashing those builds is the same as when flashing other firmware and is described in the README for each board.

After you flash CircuitPython onto your board, when you connect it to your computer, a drive named "CIRCUITPY" will appear. Copy all the *.py files from this folder to that drive, then disconnect and reconnect your board. It should now work as a game controller.

[^1]: Because these aren't official builds, technically we shouldn't refer to them as "CircuitPython". You can find the Flatbox-specific modifications [here](https://github.com/jfedor2/circuitpython/tree/flatbox).