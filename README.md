# Self-Deleting Linux Binary

This repository contains the code for a proof-of-concept program that deletes its binary content from the filesystem (and from ``/proc/<pid>/exe``) but still works.

## Usage

To use this program, simply compile the code and run the resulting executable file.\
The program will delete itself from the filesystem and from ``/proc/<pid>/exe``, but will continue to run normally until it exits.\

## Credits

The assembly code used in remapper_utils.h was provided by [@0xC_MONKEY](https://github.com/0xC-M0NK3Y).

## Known Issues

There is currently an issue with 32-bit support, as a call to ``__x86.get_pc_thunk.ax`` is popping to the assembly code.\
If anyone knows how to patch this issue, please feel free to submit a pull request.

## License

This program is released under the MIT License. See the LICENSE file for more information.

## Disclaimer

This program is intended for educational and research purposes only. It is a proof-of-concept and is not intended for malicious use.\
The author is not responsible for any misuse or damage caused by this program. Use at your own risk.