/*
 * Copyright (c) 2016
 * All rights reserved.
 * 
 * 文件名称: myfec.h
 * 文件标识:
 * 摘    要:  定义fec的功能函数，部分代码抄自www.openfec.org
 *            采用LDPC算法，来源于RFC5170
 *
 * 当前版本: 1.0
 * 作    者: chen.qian.jiang
 * 开始日期: 2016-05-24
 * 完成日期: 
 * 其它说明： 
 * 修改日期        版本号     修改人          修改内容
 * -----------------------------------------------
 * 2016/05/24      V1.0       陈黔江          创建
 */ 

#ifndef _MYFEC__H
#define _MYFEC__H



extern "C"
{
#include "../lib_common/of_openfec_api.h"
}

#pragma pack(push, 1)

//一个UDP的包长
#define  LAN_LEN  1472   //局域网 1500-28
#define  LAN_DATA 1464   //去掉头
#define  IT_LEN   548    //互联网 576-28
#define  IT_DATA  540    //去掉头 


//#define WAIT_BUF   3    //用3个buf来接收fec包
#define TYPE_I  2
#define TYPE_SPLIT 0xfff 
#define MAX_FEC_BUF 1500

#define MAX_NOENCBUF_LEN  51200
#define SINGLE_NOENCBUF_LEN  512
/*
  初始参数
*/
struct config_t
{
	unsigned int  src_num;
	unsigned int  repair_num;
	unsigned int  pkg_len;

// LDPC_Ssaircase
	unsigned char N1;
    int prng_seed;

//rs_2_m   bit size
	unsigned short	m;

//personal information
//本机的userid
	int  userId;

// identifier of the codec to use 
//	codec 是编解码ID, 目前支持如下值：
//		OF_CODEC_REED_SOLOMON_GF_2_M_STABLE
//		OF_CODEC_LDPC_STAIRCASE_STABLE
	of_codec_id_t	codecId;		

//	debug_level 显示debug信息的级别：
//		0 : no trace
//		1 : main traces
//		2 : full traces with packet dumps
	unsigned int   debug_Level;

// 用来判断序列号是否正常，是否需要reset。
// 若是current seq - m_Index > reset_num ,就reset
	int reset_num;
};

/*
 fec包头
*/
struct head_t
{
    int   user_id;
	unsigned int   packet_id;
};

// no fec buf infomation
struct nofecbuf_t
{
	short len;
	unsigned char buf[SINGLE_NOENCBUF_LEN];
};


#pragma pack(push)
#pragma pack(8)
struct packet_head_t 
{                            //最紧凑的数据包头,每个数据包只使用8字节 包括 1字节的类型; 3字节的id; 1字节的opt选项; 3字节的大小, 最大256 * 64k 
	unsigned int type_id;
	unsigned int opt_size;
	inline unsigned int get_type() const { return type_id >> 24; }
	inline void set_type(unsigned int type) { type_id = (type_id & 0x00fffffful) | (type << 24); }
	inline unsigned int get_id() const { return type_id & 0x00fffffful; }
	inline void set_id(unsigned int id) { type_id = (id & 0x00fffffful) | (type_id & 0xff000000ul); }
	inline unsigned int get_opt() const { return opt_size >> 24; }
	inline void set_opt(unsigned int opt) { opt_size = (opt_size & 0x00fffffful) | (opt << 24); }
	inline unsigned int get_size() const { return opt_size & 0x00fffffful; }
	inline void set_size(unsigned int size) { opt_size = (size & 0x00fffffful) | (opt_size & 0xff000000ul); }
};
#pragma pack(pop)


class MyFec
{
public:
	/*
	  config  设置编解码参数
	*/	
	MyFec( config_t config);

	~MyFec();

	int init_Fec();
	int reinit(config_t config);

	// flag 0 表示没有包头
	//      1 表示有包头的普通包
	//      2 表示有包头的视频A帧 
	int encode_Data(unsigned char *buf, int len , int head_flag);  
	int decode_Data(unsigned char *buf, int len );
    
	int set_no_encdata(unsigned char *buf, int len);
	int get_no_encdata(unsigned char *buf);

	//flag = 1  源包
	// flag = 0 属于校验包
	virtual void handle_single_encode_src( unsigned char *buf, int len,int flag);	
	// flag = -1  上一个包被丢弃
	// flag = 1  属于不能恢复的fec残包
	// flag = 0  属于正常恢复的fec包
	virtual void handle_single_decode_src( unsigned char *buf, int len, int flag); 

	// 用于声音包的推送
	virtual void handle_audio();
	
	int get_feclost();

	//用于统计复原率
	int   m_sta_num;                //成功复原数
	int   m_err_num;                //失败复原数

	//临时统计声音传输数据

	/*int  m_ErrPacket_Num = 0;
	int  m_Set_Audi_Err_Num = 0;
	int  m_SendBuf_Small_Num = 0;
	int  m_Add_Buf_Len = 0;
	int  m_Send_Buf_Len = 0;*/

private:

	int  create_Enc_Repair(); // create Enc Repair

	int  push_DecBuf(int len);
	int  repair_DecBuf(int id, int flag,int len);
	unsigned short  set_headflag(int headflag, int len);
	void release();
	int push_IncompleteBuf(int  len);

	config_t  m_Config;            //编码和解码的设置参数

	unsigned int  m_Seq;           //从0开始的序列号

	of_parameters_t	*m_Params;
	
	of_session_t	*m_Encode_Ses;		/* openfec codec instance identifier */
	void**		m_Enc_symbols_tab;      // packet no inlcude head
	void**		m_Enc_send_symbols_tab; // pakcet include head
	unsigned char *m_Send_Buf;         //用于发送的缓冲
	int  m_Send_Buflen;                //设置发送缓冲的结尾处
	int  m_Enc_Index;                  //标记编码是否可以开始构建检验包
	unsigned short  m_Head_Flag;         //包头偏移位置


	of_session_t	* m_Decode_Ses;	     /* openfec codec instance identifier */
	void**		m_Dec_symbols_tab;   //用于保存待解码的数据包	
	unsigned int       m_Index;      //对应每一个待解码的数组的最小序列号       
	unsigned int       m_Dec_Num;    //待解码的包数

	//unsigned char *m_Recv_Buf;         //用于接收的缓冲
	//int  m_Recv_Buflen;                //设置接收缓冲的结尾处
	int m_Pushed_seq;                 //标记已经被推出给回调者的buf 的	seq位置
	unsigned char *m_Hash_src;      //用于处理快速推出buf的hash表，1为已经拿到，0为空的
	int m_Lost_flag;                         //提示上一个session的包被丢弃 
	bool m_isI;                     //表明这个block有I帧	
	//用于保存不需要编码的数据
	unsigned char *m_Send_Noenc_Buf;
	unsigned char *m_Recv_Noenc_Buf;
	//取得长度为len的不需编码的buf
	int get_send_noenc_buf(unsigned char* buf);
	int fill_recv_noenc_buf(struct nofecbuf_t &noenc);
	int m_sendbuf_begin;
	int m_sendbuf_end;
	int m_recvbuf_begin;
	int m_recvbuf_end;
	//需要顺序取得不需编码的buf
	struct nofecbuf_t*  m_Noenc_Recv;	
};

#pragma pack(pop)
#endif
