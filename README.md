# e-CzasPL Radio reference C++ data decoder by SP6HFE

This project consist of GNU Radio Companion flow graph to capture 225[kHz] AM boadcast and reference software decoder allowing to extract time frames encoded in broadcast emission using phase shift modulation of its carrier frequency. A time signals broadcasting station is Polish National Radio emiting its Program Pierwszy Polskiego Radia radio program. Due to high transmitting power (1[MW]) signal easily cover whole Poland allowing for Polish official time UTC(PL) dissemination throughout the country. Due to ionospheric reflections radio signal can be received worldwide and that is great because you could learn from this project hands-on even you're not living in Poland.

Project intention is to showcase, in an open source way, complete signal processing path from the radiowaves to useful data.

Project is still under development and you're welcome to contribute.

## GNU Radio flow

In order to obtain digital data stream from which it would be possible to extract useful data it is needed to acquire RF data and translate it into something meaningful. A GNU Radio Companion flow (*e-zas.grc*) is available in `external/sp5wwp/e-Czas` folder (this is a git submodule) allowing to sample and process radio signal. It should constantly receive I/Q data stream from SDR receiver but for the sake of development it is possible to analyze radio station's carrier pre-recorded. A sample file `224k_1836.wav` is provided in the `data` directory.

When `.grc` file is run using GNU Radio it will create an output `dump.wav` file which is an input that is accepted by the decoder.

## Building C++ decoder

In order to build the C++ application `make` tool is used.

`make` does the compilation while `make clean` remove all the building process artifacts.

Special build which outputs a bit more to the console is available with `make debug` while speed-optimized one is `make release`.

Result of compilation is an executable located in `build/apps` called `eCzasPL`.

## Running the C++ decoder

To run a decoder against a data stream it is needed to pipe input data via standard input.

For testing purposes there is an example dump file in `data/` folder called `dump_cropped.raw`. This file is a result of running a GRC flow.

Example use of the dump file with the decoder (assuming you're in the project's top folder): `cat /data/dump_cropped.raw | ./build/apps/eCzasPL`

## Authors and contributors

* Grzegorz SP6HFE - Initial implementation of the C++ decoder
* Wojciech SP5WWP - GRC + initial implementation of the data decoder in C (see submodule)