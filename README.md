# PlistBuddy
Function-to-function identical implementation of `/usr/libexec/PlistBuddy` for non-macOS

### Current Status

Aligned with macOS 13 shipped implementation.
Minimal target has not been tested yet, but should at least supporting Darwin 19 (macOS 10.14 / iOS 13), backports welcomed.

### TODO

- [ ] CoreFoundation-less port for non-Darwin systems

### `plutil`?

See [foundation_cmds](https://github.com/Remorix/foundation_cmds)
