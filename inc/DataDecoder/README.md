# e-CzasPL Radio RF stream data decoder

This module is responsible for retrieving [e-CzasPL Radio][1] time frames and decoding encapsulated UTC(PL) official time messages.  
Input data is a stream of samples being an information about phase change of the 225[kHz] long wave carrier signal which is broadcast by the Polish Radio (Program Pierwszy Polskiego Radia program).  
Carrier signal modulation is a PSK ([phase shift keying][2]) with phase deviation of +/- 36 degrees.  
In order to receive a meaningful data a PSK demodulator is needed to get a bitstream out of the phase information.  
Project's github is [here][3].

## Time frame

Structure of the time frame consist of 12 bytes and is is depicted below.

![alt][timeFrame]

Data stream is transferred bit-by-bit left to right, top to bottom.  
Top left bit is the MSb of the 1st byte whereas bottom right is the LSb of the 12th byte.

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

Time message consist of 37 bytes which are part of the received time frame.  
Time message structure is explained in the table below (provided its MSb is numbered here `0` which is a bit `27` in the time frame).

| Bit range                 | Information/purpose                 |
|:-------------------------:|:------------------------------------|
| 0(S0)-29(S29)             | Threes of seconds since year 2000   |
| 30(TZ0)-31(TZ1)           | Local time zone                     |
| 32(LS)                    | Leap second announcement            |
| 33(LSS)                   | Leap second sign                    |
| 34(TZC)                   | Local time zone change announcement |
| 35(SK0)-36(SK1)           | Transmission site operation state   |

### Seconds since year 2000

Within time message there is a room for 30b timestamp. Because of transmission scheme, where every new data frame is transmitted every 3[s], mentioned 30b value is actually a number of threes of seconds since the beginning of the year 2000.

An example:  
S0-S29 give the value of `258787930`. It means that a number of seconds since year 2000 is `3*258787930 = 776363790`.

In order to validate the timestamp it is possible to use some online converter [like this][4]. The problem is that it accepts a UNIX timestamp which is a number of seconds since the beginning of UNIX epoh - a year 1970. While between 1970 and 2000 `946684800` seconds elapsed a test value is simply `776363790+946684800 = 1723048590` giving a readout of `Wed Aug 07 2024 16:36:30 GMT+0000` which is correct.

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

## Time data scrambling

On transmission side bits 27 to 63 of the time frame are scrambled using 0x0A47554D2B scrambling word which in ASCII world mean `\nGUM+`.  
Out of the magic number only 37 least significant bits are used for scrambling.

Scrambling is effectively a process of bits XORing. MSb of the scrambled data is XORed with MSb of the scrambling word, next bit of the data is XORed with next bit of the scrambling word and so on.

On reception side it is needed to reverse the operation by effectively doing the same operation. This time it is called de-scrambling.

Scrambling is a form of data encryption, but whenever you publicly show the key there is no point in doing so.
Probably there will be a service, in future, where some of the time frames will be encrypted using a key shared only with trusted partners who may be signing an NDA or other agreements. This may be to increase level of trust in the synchronization achieved over the radio link.
By that time all mechanisms are already in place :)

## Reed-Solomon error correction

To be added

## CRC-8

To be added

[1]: https://e-czas.gum.gov.pl/e-czas-radio/
[2]: https://en.wikipedia.org/wiki/Phase-shift_keying
[3]: https://github.com/e-CzasPL/TimeReceiver225kHz
[4]: https://www.unixtimestamp.com/

[timeFrame]: ../../doc/img/eCzasPL_time_frame.jpg "e-CzasPL Radio time frame (source: e-CzasPL documentation)"