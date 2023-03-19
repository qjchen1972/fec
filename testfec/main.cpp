#include<string.h>
#include <stdlib.h>
#include <iostream>
#include<fstream>
#include<time.h>


#include "../myfec/myfec.h"

using namespace std;
//using std::ifstream; 
//using std::ofstream;

unsigned char sendbuf[400*548];
int  sendlen;
int  s_send = 0;
int  r_send = 0;

class testfec : public MyFec
{
public:
	testfec(config_t config) : MyFec(config)
	{
		m_c = config;
	}

	void handle_single_encode_src(unsigned char *buf, int len, int flag)
	{
		 
		unsigned char tt[8];

		ofstream outfile;

		ofstream sfile;

		
		memset(tt, 14, sizeof(tt));
		outfile.open("encode",  ios::binary| ios_base::app);  
		if (outfile.is_open())
		{
			if (!flag) outfile.write((char*)tt, sizeof(tt));
			outfile.write((char*)buf, len);			
			outfile.close();
		}

		unsigned int seq;
		memcpy(&seq, buf + 4, 4);
		
		int kk = rand() % 100;
		char mm[300];
		

		if (  seq >= 0  )
		{
			decode_Data(buf, len);
			s_send++;
			//sprintf(mm, " code is  %d  rand is %d , send %d, total %d\n", m_c.codecId, kk, seq, s_send);
			//printf("%s \n", mm);

		}
		
		//memcpy(sendbuf + sendlen, buf, len);
		//sendlen += len;
		//unsigned int seq;
		//memcpy(&seq, buf + 4, 4);
		//char mm[300];
		//sprintf(mm, "rand is %d , send %d, total %d\n", kk, seq, total_send);
		sfile.open("log", ios_base::app);
		if (sfile.is_open())
		{
			//if (!flag) outfile.write((char*)tt, sizeof(tt));
			//sfile.write(mm, strlen(mm));
			sfile.close();
		}
	}

	void handle_single_decode_src(unsigned char *buf, int len, int flag)
	{
		unsigned char tt[8];
		ofstream outfile;

		memset(tt, 0xff, sizeof(tt));
		char mm[300];
		unsigned short p;

		memcpy(&p, buf, 2);
		sprintf(mm, "recv  %d  flag is %d \n", p, flag);
		if(flag == -1)		printf("%s \n", mm);
		r_send++;
		outfile.open("decode", ios::binary | ios_base::app);  
		if (outfile.is_open())
		{
			outfile.write((char*)tt, sizeof(tt));
			outfile.write((char*)buf, len);
			outfile.close();
		}
	}
	
public:
	config_t m_c;
};




int main()
{
	int  ii;
	unsigned char *buf;
	buf = new unsigned char[300*538];

	for (ii = 0; ii < 300 * 538; ii++)
	{
		buf[ii] = ii%0xff;		
	}

	
	

	config_t set;
	set.src_num =220;
	set.repair_num = 30;
	set.pkg_len = 548;
	//OF_CODEC_REED_SOLOMON_GF_2_M_STABLE
	set.N1 = 7;
	set.prng_seed = 134678921;
	
	//		OF_CODEC_LDPC_STAIRCASE_STABLE
	//set.codecId = OF_CODEC_LDPC_STAIRCASE_STABLE;
	set.m = 8;
	set.codecId = OF_CODEC_REED_SOLOMON_GF_2_M_STABLE;
	set.debug_Level = 2;
	set.userId = 1;

	testfec f(set);
	int r = f.init_Fec();
	if (r< 0) 
	{
		printf("error\n");
		return 0;
	}

	sendlen = 0;
	int num = 0;

	srand((unsigned)time(NULL));
	int k;
	int ttt = 0;


	time_t start, stop;

	start = time(NULL);

	for (k = 0; k < 200; k++)
	{
		s_send = 0;
		r_send = 0;
		num = 0;

		int p;


		for (ii = 0; ii < 538; ii++)
		{
			p = rand() % 2;
			f.encode_Data((unsigned char*)buf + num, 20, p);
			num += 20;
		}

		/*f.Encode_Data( (unsigned char*)buf,87,1);
		num += 87;
		f.Encode_Data((unsigned char*)buf, 187,1);
		num += 187;
		f.Encode_Data((unsigned char*)buf, 387,1);
		num += 387;*/
		f.encode_Data((unsigned char*)buf+num, 220 * 538 - num, 1);

		printf(" code is %d  send is %d  recv is %d \n", set.codecId, s_send, r_send);
		if (s_send >= set.src_num) ttt++;
	}
	stop = time(NULL);
	printf("Use Time:%ld\n", (stop - start));

	printf(" succed is %d\n", ttt);


	//f.Decode_Data(testbuf, testlen);

	return 1;
	
}