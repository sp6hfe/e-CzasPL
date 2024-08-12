#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

const int8_t sync[16]={-1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1}; //sync symbol transitions
int16_t s[12*8*10+1]; //a whole 1.92s frame should fit in (50bps, 10 samples per symbol)
uint8_t skip_samples;
uint16_t skip_cnt;

int main(void)
{
	while(1)
	{
		while(fread((uint8_t*)&s[sizeof(s)/sizeof(int16_t)-1], sizeof(int16_t), 1, stdin)<1);

		//shift left
		for(uint16_t i=0; i<sizeof(s)/sizeof(int16_t)-1; i++)
			s[i]=s[i+1];

		if(!skip_samples)
		{
			//correlate against syncword
			int32_t corr=0;
			for(uint16_t i=0; i<16*10; i+=10)
				corr+=s[i]*sync[i/10];

			if(corr>320000 && s[0]<-10000)
			{
				uint8_t b=0;

				for(uint16_t i=0; i<96; i++)
				{
					printf("%d", b); //quirky?

					if(abs(s[i*10])>10000)
						b=!b;

					if(i>0 && !((i+1)%8))
						printf(" ");
				}
				printf("\n");

				skip_samples=1;
				skip_cnt=0;
			}
		}
		else
		{
			skip_cnt++;
			if(skip_cnt==12*8*10)
			{
				skip_cnt=0;
				skip_samples=0;
			}
		}
	}

	return 0;
}
