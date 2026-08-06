# Hardware harness

Scripts used to verify the encoders against real printers over USB.  Not part
of the library or its build; delete if the printers go away.

    uv run --with pyusb python -c "...")

- `usbio.py`   claim a USB printer-class device, bulk write, read a fixed reply
- `pattern.py` full-label test card: frame, corner L marks, diagonals,
               10 mm ruler ticks, solid block, checkerboard
- `hangul.py`  a label packed with Hangul at shrinking sizes - the glyphs are
               dense enough to show where strokes start dropping out
- `raster.py`  Brother QL raster job builder, kept as the reference the C
               encoder was diffed against
- `query.py`   ask a printer what it is - TSPL and ZPL queries, no printing
- `ruler.py`   numbered TSPL ruler that overshoots the label: the last legible
               number is the printable size, whatever the printer claims

macOS: `ippusbd` grabs the Brother the moment it enumerates.  `sudo killall
ippusbd`, then claim without resetting the device - a reset re-enumerates it
and launchd starts the daemon again.  After claiming, force alt setting 0:
`d.set_interface_altsetting(interface=0, alternate_setting=0)`.
