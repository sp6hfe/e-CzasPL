# e-CzasPL Radio RF stream data decoder

This module is responsible for decoding UTC(PL) official time ensapsulated in time messages sent over long wave frequency of 225[kHz] using carrier phase modulation.

## Scrambling

On tramsmission side bits 27 to 63 of the time frame are scrambled using 0x0A47554D2B scrambling word. Scrambling is effectively a process of bits XORing. MSb of the scrambled data is XORed with MSb of the scrambling word, next bit of the data is XORed with next bit of the scrambling word and so on.

On reception side it is needed to reverse the operation by effectively doing the same operation. This time it is called de-scrambling.

Scrambling is a form of data encryption, but whenever you publicly know the key there is no point in doing so.
Probably there will be a service, in future, where some of the time frames will be encrypted using a key shared to only trusted partners who may be signing an NDA or other agreements. This may be to increase level of trust in the synchronization ahieved over radio link.
By that time all mechanisms are already in place :)