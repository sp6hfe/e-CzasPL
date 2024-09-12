# e-CzasPL Radio RF stream data decoder

This module is responsible for retrieving [e-CzasPL Radio][1] time frames and decoding encapsulated UTC(PL) official time messages. Input data is a stream of samples being an information about phase change of the 225[kHz] carrier signal of Polish Radio Program 1 long wave broadcast.

## Time frame

Structure of the time frame consist of 12 bytes and is is depicted below:

![alt][timeFrame]

Data stream is transferred bit by bit left to right, top to bottom.  
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

## Data scrambling

On transmission side bits 27 to 63 of the time frame are scrambled using 0x0A47554D2B scrambling word which in ASCII world mean `\nGUM+`. Out of the magic number only 37 least significant bits are used for scrambling.

Scrambling is effectively a process of bits XORing. MSb of the scrambled data is XORed with MSb of the scrambling word, next bit of the data is XORed with next bit of the scrambling word and so on.

On reception side it is needed to reverse the operation by effectively doing the same operation. This time it is called de-scrambling.

Scrambling is a form of data encryption, but whenever you publicly know the key there is no point in doing so.
Probably there will be a service, in future, where some of the time frames will be encrypted using a key shared to only trusted partners who may be signing an NDA or other agreements. This may be to increase level of trust in the synchronization ahieved over radio link.
By that time all mechanisms are already in place :)

## Reed-Solomon error correction

To be added

## CRC-8

To be added

[1]: https://github.com/e-CzasPL/TimeReceiver225kHz
[2]: https://www.unixtimestamp.com/

[timeFrame]: ../../doc/img/eCzasPL_time_frame.jpg "e-CzasPL Radio time frame (source: e-CzasPL documentation)"