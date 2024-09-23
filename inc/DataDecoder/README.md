# e-CzasPL Radio RF stream data decoder

This module is responsible for retrieving [e-CzasPL Radio][1] time frames and decoding encapsulated UTC(PL) official time messages.  
Input data is a stream of samples being an information about phase change of the 225[kHz] long wave carrier signal which is broadcast by the Polish Radio (Program Pierwszy Polskiego Radia program).  
Carrier signal modulation is a PSK ([phase shift keying][2]) with phase deviation of +/- 36 degrees.  
In order to receive a meaningful data a PSK demodulator is needed to get a bitstream out of the phase information.  
Project's github is [here][3].

## Time frame

Time frame consist of 12 bytes numbered from `0` to `11` and 0th byte being the MSB.  
Frame structure is depicted below.

![alt][timeFrame]

Data stream is transferred bit-by-bit left to right, top to bottom.  
Top left bit is the MSb of the 1st byte (no `0`) whereas bottom right is the LSb of the 12th byte (no `11`).

Contentwise 1st transmited bit is numbered `0` and the last one `95`. Such stream is 96b long which make 12B of data.  
Time frame structure is explained in the table below.

| Bit range                 | Information/purpose                |
|:-------------------------:|:-----------------------------------|
| 0-15                      | Frame sync word 0x5555             |
| 16-23                     | Time message ID 0x60               |
| 24-26                     | Static value 0b101                 |
| 27(S0)-63(SK1)            | Time message                       |
| 64(ECC0 MSb)-87(ECC2 LSb) | Reed-Solomon error correction data |
| 88-95                     | CRC-8                              |

## Time message

Time message consist of 37 bits which are part of the received time frame.  
Time message structure is explained in the table below (provided its MSb is numbered here `0` which is a bit `27` in the time frame).

| Bit range                 | Information/purpose                            |
|:-------------------------:|:-----------------------------------------------|
| 0(S0)-29(S29)             | 3-second periods since 01.01.2000 00:00:00 UTC |
| 30(TZ0)-31(TZ1)           | Local time zone                                |
| 32(LS)                    | Leap second announcement                       |
| 33(LSS)                   | Leap second sign                               |
| 34(TZC)                   | Local time zone change announcement            |
| 35(SK0)-36(SK1)           | Transmission site operation state              |

### Seconds since year 2000

Within time message there is a room for 30b timestamp. Because of transmission scheme, where every new data frame is transmitted every 3[s], mentioned 30b value is actually a number of threes of seconds since the beginning of the year 2000.

An example:  
S0-S29 give the value of `258787930`. It means that a number of seconds since year 2000 is `3*258787930 = 776363790`.

In order to validate the timestamp it is possible to use some online converter [like this][4]. The problem is that it accepts a UNIX timestamp which is a number of seconds since the beginning of UNIX epoch - a year 1970. While between 1970 and 2000 `946684800` seconds elapsed a test value is simply `776363790+946684800 = 1723048590` giving a readout of `Wed Aug 07 2024 16:36:30 GMT+0000` which is correct.

### Local time zone

Local time zone for the transmission site changes thoughout the year (summer/winter time).  
Even though the current time is sent as UTC(PL) all radio signal recipients may adjust their clocks to the local time zone.  
An offset, in hours, to UTC(PL) encoded on bits `TZ0` and `TZ1` is described in the table below.

| TZ0 | TZ1 | Offset to UTC(PL) |
|:---:|:---:|:------------------|
|  0  |  0  | +0 hours          |
|  1  |  0  | +1 hour           |
|  0  |  1  | +2 hours          |
|  1  |  1  | +3 hours          |

### Local time zone change announcement

Twice a year a local time zone ich changed. This is correlated with a seasons and amount of sunlight available during the day.  
Changes are performed on last Sunday in March and on last Sunday in October.  
In order to mark such an upcoming event bit `TZC` is being set for few days (up to 6) prior to change.  

### Leap second announcement

On an occasion a leap second is added to UTC (there is a discussion to stop that activity at all as there are real issues when time is not continous).  
In order to mark such an upcoming event bit `LS` is being set for few days up to 1 week prior to change.  
Leap second can be introduced on 1st of January, 1st of April, 1st of July or on 1st of October.  
Change is effective at `00:00:00 UTC(PL)`.

### Leap second sign

When leap second is to be added to UTC(PL) value of bit `LSS` decide about its sign.  
When `LSS` bit is 0 there will be additional second inserted (00:00:00 will last 2 seconds) whereas value of 1 mean that one second will be dropped (instead of 00:00:00 we would have 00:00:01).

### State of the transmitter

From time to time there might be some down times for the transmission due to planned maintenance works.  
Schedule for such an event is encoded on bits `SK0` and `SK1` and described in the table below.

| SK0 | SK1 | Transmitter state                   |
|:---:|:---:|:------------------------------------|
|  0  |  0  | Normal operation                    |
|  1  |  0  | Planned 1 day maintenance           |
|  0  |  1  | Planned 1 week maintenance          |
|  1  |  1  | Planned maintenance for over 1 week |

## Reed-Solomon error correction

In order to strengthen time information reception a Reed-Solomon error correction algorithm is used.  
Time frame bytes `8`(ECC0) to `10`(ECC2) consist of 6 redundant 4-bit symbols which are added in order to allow recovery of possibly corrupted bits `27`S0 to `35`SK0 (bit `36`(SK1) is not covered).  

Reed-Solomon error correction bytes are calculated over extended Galois field `GF(2)` (due to binary nature of transmitted data).  

Important information to know is:
* the smallest chunk of data is called *a symbol* and it is selected to be `m=4` bits long (thus Galois field used here is `GF(2^4)=GF(16)`),
* maximum length of code word (a sentence secured with error correction) is `n=15` symbols long (2^m-1),
* within that code word `k=9` symbols is dedicated to time data (we should cover 37 bits of time message but it would require one bit from additional symbol),
* having code word size of `15` symbols and useful data size of `9` symbols what's left is `6` symbols for FEC data,
* such additional 6 symbols for error correction are called `2t` giving the possibility to recover up to any `t=3` corrupted symbols from the entire code word ((n-k)/2),
* `2t` symbols of FEC data costs `2\*3\*4 bits = 24 bits`,
* above configuration of Reed-Solomon error correction coding is called `RS(15,9)`,
* primitive polynomial used for Galois field initialization is `x^2+x+1` (decimal 19),
* primitive element used for Galois field initialization is `x` (decimal 2),
* polynomial roots generator has initial root `b=1`,
* transmitted information (code word) is represented by the coefficients of the polymomial of the order `n-1` (14).

Reed-Solomon RS(15,9) decoding algorithm run over time message data S0-SK0 concatenated with ECC0-ECC2 should either correct errors or fail.

Received 4 bit chunks are coefficients of the code word's polynomial organized as below:
* value of 4-bit symbol LS-SK0 is a coefficient of `x^14`,
* value of 4-bit symbol S0-S3 is a coefficient of `x^6`,
* value of 4-bit symbol (lower part of ECC2) is a coefficient of `x^5`,
* value of 4-bit symbol (upper part of ECC0) is a coefficient of `x^0`.


To play around with the data [here][6] is a good resource.  
A good explanation of the topic is [here][7], [here][8] and [here][9].  
Some other valuable resources I have found interesting are [here][10], [here][11], [here][12], and [here][13].

## CRC-8

CRC8 checksum is calculated over time frame bytes `3` to `7` including (remember we number them from `0` to `11`).  
CRC8 allows for estimation if time message data was received without issues.  
Calculated checksum is verified against value received in frame's 11th byte.  
In case of inconsistency there is no other means of recovering the original data.

CRC8 calculation is widely described (i.e. [here][5]) and important information to know is:
* polynomial: `0x07`
* initialization value: `0x00`
* checksum is calculated `over scrambled data` (meaning data validation is pretty straight-forward)

## Time data scrambling

On transmission side bits 27 to 63 of the time frame are scrambled using 0x0A47554D2B scrambling word which in ASCII world mean `\nGUM+`.  
Out of the magic number only 37 least significant bits are used for scrambling.

Scrambling is effectively a process of bits XORing. MSb of the scrambled data is XORed with MSb of the scrambling word, next bit of the data is XORed with next bit of the scrambling word and so on.

On reception side it is needed to reverse the operation by effectively doing the same operation. This time it is called de-scrambling.

Scrambling is a form of data encryption, but whenever you publicly show the key there is no point in doing so.
Probably there will be a service, in future, where some of the time frames will be encrypted using a key shared only with trusted partners who may be signing an NDA or other agreements. This may be to increase level of trust in the synchronization achieved over the radio link.
By that time all mechanisms are already in place :)


[1]: https://e-czas.gum.gov.pl/e-czas-radio/
[2]: https://en.wikipedia.org/wiki/Phase-shift_keying
[3]: https://github.com/e-CzasPL/TimeReceiver225kHz
[4]: https://www.unixtimestamp.com/
[5]: http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html
[6]: https://www.ujamjar.com/demo/ocaml/2014/06/18/reed-solomon-demo.html
[7]: https://ntrs.nasa.gov/api/citations/19900019023/downloads/19900019023.pdf
[8]: https://berthub.eu/articles/posts/reed-solomon-for-programmers/
[9]: https://siglead.com/en/technology-eg/reed-solomoncode/
[10]: https://mathworld.wolfram.com/PrimitivePolynomial.html
[110]: https://core.ac.uk/download/pdf/16697418.pdf
[11]: https://aspur.rs/jemit/archive/v3/n3/7.pdf
[13]: http://www.iraj.in/journal/journal_file/journal_pdf/1-605-15767466889-13.pdf

[timeFrame]: ../../doc/img/eCzasPL_time_frame.jpg "e-CzasPL Radio time frame (source: e-CzasPL documentation)"