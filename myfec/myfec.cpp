/*
 * Copyright (c) 2016
 * All rights reserved.
 * 
 * 文件名称: myfec.c
 * 文件标识:
 * 摘    要:  实现fec的功能函数，部分代码抄自www.openfec.org
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
#include<string.h>
#include <stdlib.h>
#include<stdio.h>


#include "myfec.h"


MyFec::MyFec( config_t config )
	
{
	int i;

	memcpy(&m_Config, &config, sizeof(config_t));
	m_Config.pkg_len = m_Config.pkg_len - sizeof(head_t);
	
	m_Seq = 0;

	//encode
	m_Encode_Ses = NULL;
	m_Enc_symbols_tab = NULL;
	m_Enc_send_symbols_tab = NULL;
	m_Send_Buf = NULL;
	m_Send_Buflen = 0;
	m_Enc_Index = 0;
	m_Head_Flag = TYPE_SPLIT;


	m_Dec_symbols_tab = NULL;
	m_Hash_src = NULL;
	m_Decode_Ses = NULL;
	
	m_Index = 0;
	m_Dec_Num = 0;
	m_Pushed_seq = 0;
	
	m_Params = NULL;
	m_Lost_flag = -1;

	of_verbosity = m_Config.debug_Level;
	m_isI = false;
	m_sta_num = 0;
	m_err_num = 0;

	m_Send_Noenc_Buf = NULL;
	m_Recv_Noenc_Buf = NULL;
	m_sendbuf_begin = 0;
	m_sendbuf_end = 0;
	m_recvbuf_begin = 0;
	m_recvbuf_end = 0;
	m_Noenc_Recv = NULL;
}


int MyFec::init_Fec( )
{	
	int esi;
	int total_num ;	

	switch (m_Config.codecId)
	{
	case OF_CODEC_REED_SOLOMON_GF_2_M_STABLE:
	{
		of_rs_2_m_parameters_t	*my_params;
		if ((my_params = (of_rs_2_m_parameters_t *)calloc(1, sizeof(*my_params))) == NULL)
		{
			OF_PRINT_LVL(1,("no memory for codec %d\n", m_Config.codecId))
			return  -1;
		}
		my_params->m = m_Config.m;
		m_Params = (of_parameters_t *)my_params;
		break;
	}
	case OF_CODEC_LDPC_STAIRCASE_STABLE:
	{
		of_ldpc_parameters_t	*my_params;
		if ((my_params = (of_ldpc_parameters_t *)calloc(1, sizeof(*my_params))) == NULL)
		{
			OF_PRINT_LVL(1,("no memory for codec %d\n", m_Config.codecId))
			return  -1;
		}
		my_params->prng_seed = m_Config.prng_seed;
		my_params->N1 = m_Config.N1;
		m_Params = (of_parameters_t *)my_params;
		break;
	}
	default:
		OF_PRINT_LVL(1, ("Invalid FEC OTI received: codec_id=%u \n", m_Config.codecId))
		return  -1;
	}

	m_Params->nb_source_symbols	= m_Config.src_num;		/* fill in the generic part of the of_parameters_t structure */
	m_Params->nb_repair_symbols	= m_Config.repair_num;
	m_Params->encoding_symbol_length	= m_Config.pkg_len;
	
	//OF_PRINT_LVL(1, ("fec is   %d   %d %d \n", m_Config.src_num, m_Config.repair_num, m_Config.pkg_len))

	total_num = m_Config.src_num + m_Config.repair_num;

	/* Open and initialize the openfec session now... */
	if ( of_create_codec_instance(&m_Encode_Ses, m_Config.codecId, OF_ENCODER, of_verbosity) != OF_STATUS_OK)
	{
		OF_PRINT_LVL(1, ("%d of_create_codec_instance() failed\n",m_Config.userId ))
		return  -1;
	}
	if (of_set_fec_parameters(m_Encode_Ses, m_Params) != OF_STATUS_OK)
	{
		OF_PRINT_LVL(1, ("%d of_set_fec_parameters() failed for codec_id \n", m_Config.userId))
		return  -1;
	}	

	if ((m_Enc_symbols_tab = (void**) calloc(total_num, sizeof(void*))) == NULL)
	{
		OF_PRINT_LVL(1, ("no memory (calloc failed for enc_symbols_tab, n=%u)\n", total_num))
		return  -1;
	}

	if ((m_Enc_send_symbols_tab = (void**) calloc(total_num, sizeof(void*))) == NULL)
	{
		OF_PRINT_LVL(1, ("no memory (calloc failed for enc_send__symbols_tab, n=%u)\n", total_num))
		return  -1;
	}


	if ((m_Send_Buf = (unsigned char*)calloc(  m_Config.pkg_len, sizeof(unsigned char))) == NULL)
	{
		OF_PRINT_LVL(1, ("no memory (calloc failed for m_Send_Buf)\n"))
		return  -1;
	}

	if ((m_Dec_symbols_tab = (void**)calloc(total_num, sizeof(void*))) == NULL)
	{
		OF_PRINT_LVL(1, ("no memory (calloc failed for m_Dec_symbols_tab, n=%u)\n", total_num))
			return  -1;
	}
	if ((m_Hash_src = (unsigned char*)calloc(total_num, sizeof(unsigned char))) == NULL)
	{
		OF_PRINT_LVL(1, ("no memory (calloc failed for hash)\n"))
			return  -1;
	}
	memset(m_Hash_src, 0, sizeof(unsigned char)*total_num);

	if ((m_Noenc_Recv = (struct nofecbuf_t*)calloc(m_Config.src_num, sizeof(struct nofecbuf_t))) == NULL)
	{
		OF_PRINT_LVL(1, ("no memory (calloc failed for m_Noenc_Recv, n=%u)\n", m_Config.src_num))
			return  -1;
	}			
	
	for (esi = 0; esi < total_num ; esi++ )
	{
		if ((m_Enc_symbols_tab[esi] = (unsigned char*)calloc(m_Config.pkg_len, sizeof(unsigned char))) == NULL)
		{
			OF_PRINT_LVL(1, ("no memory (calloc failed for m_Enc_symbols_tab[%d])\n", esi))
			return  -1;
		}
		if ((m_Enc_send_symbols_tab[esi] = (unsigned char*)calloc(/*m_Config.pkg_len + sizeof(head_t)*/LAN_LEN, sizeof(unsigned char))) == NULL)
		{
			OF_PRINT_LVL(1, ("no memory (calloc failed for m_Enc_send_symbols_tab[%d])\n", esi))
			return  -1;
		}

		if ((m_Dec_symbols_tab[esi] = (unsigned char*)calloc(/*MAX_FEC_BUF*/m_Config.pkg_len, sizeof(unsigned char))) == NULL)
		{
			OF_PRINT_LVL(1, ("no memory (calloc failed for m_Dec_symbols_tab[%d])\n", esi))
				return  -1;
		}		
	}
	if ((m_Send_Noenc_Buf = (unsigned char*)calloc(MAX_NOENCBUF_LEN, sizeof(unsigned char))) == NULL)
	{
		OF_PRINT_LVL(1, ("no memory (calloc failed for m_Send_Noenc_Buf)\n"))
			return  -1;
	}

	if ((m_Recv_Noenc_Buf = (unsigned char*)calloc(MAX_NOENCBUF_LEN, sizeof(unsigned char))) == NULL)
	{
		OF_PRINT_LVL(1, ("no memory (calloc failed for m_Recv_Noenc_Buf)\n"))
			return  -1;
	}

	return 1;

}

void MyFec::release()
{
	int esi;
	int total_num = m_Config.src_num + m_Config.repair_num;

	if (m_Encode_Ses)
	{
		of_release_codec_instance(m_Encode_Ses);
	}
	
	if (m_Params)
	{
		free(m_Params);
	}

	if (m_Enc_symbols_tab)
	{
		for (esi = 0; esi < total_num; esi++)
		{
			if (m_Enc_symbols_tab[esi])
			{
				free(m_Enc_symbols_tab[esi]);
			}
		}
		free(m_Enc_symbols_tab);
	}
	if (m_Enc_send_symbols_tab)
	{
		for (esi = 0; esi < total_num; esi++)
		{
			if (m_Enc_send_symbols_tab[esi])
			{
				free(m_Enc_send_symbols_tab[esi]);
			}
		}
		free(m_Enc_send_symbols_tab);
	}

	if (m_Dec_symbols_tab)
	{
		for (esi = 0; esi < total_num; esi++)
		{
			if (m_Dec_symbols_tab[esi])
			{
				free(m_Dec_symbols_tab[esi]);
			}
		}
		free(m_Dec_symbols_tab);
	}

	if (m_Hash_src) free(m_Hash_src);
	if (m_Decode_Ses)
	{
		of_release_codec_instance(m_Decode_Ses);
	}

	if (m_Send_Buf)
	{
		free(m_Send_Buf);
	}

	if (m_Send_Noenc_Buf)
	{
		free(m_Send_Noenc_Buf);
	}

	if (m_Recv_Noenc_Buf)
	{
		free(m_Recv_Noenc_Buf);
	}

	if ( m_Noenc_Recv )
	{
		free(m_Noenc_Recv);
	}		
}

 MyFec::~MyFec()
{
	release();			
}

 int MyFec::reinit(config_t config)
 {
	 int i;

	 release();
	 
	 memcpy(&m_Config, &config, sizeof(config_t));
	 m_Config.pkg_len = m_Config.pkg_len - sizeof(head_t);

	 m_Seq = 0;

	 //encode
	 m_Encode_Ses = NULL;
	 m_Enc_symbols_tab = NULL;
	 m_Enc_send_symbols_tab = NULL;
	 m_Send_Buf = NULL;
	 m_Send_Buflen = 0;
	 m_Enc_Index = 0;
	 m_Head_Flag = TYPE_SPLIT;


	 m_Dec_symbols_tab = NULL;
	 m_Hash_src = NULL;
	 m_Decode_Ses = NULL;

	 m_Index = 0;
	 m_Dec_Num = 0;
	 m_Pushed_seq = 0;

	 m_Params = NULL;
	 m_Lost_flag = -1;

	 of_verbosity = m_Config.debug_Level;
	 m_isI = false;
	 m_sta_num = 0;
	 m_err_num = 0;
	 
	 m_Send_Noenc_Buf = NULL;
	 m_Recv_Noenc_Buf = NULL;
	 m_sendbuf_begin = 0;
	 m_sendbuf_end = 0;
	 m_recvbuf_begin = 0;
	 m_recvbuf_end = 0;

	 m_Noenc_Recv = NULL;

	 return init_Fec();
 }

 int MyFec::get_feclost()
 {
	 int ret;
	 if (m_err_num + m_sta_num == 0)
		 ret = 0;
	 else
		 ret = ( (float)(m_err_num * 100) / (float)(m_err_num + m_sta_num)) * 1000;
	 m_sta_num = 0;
	 m_err_num = 0;
	 return ret;
}

 // head_flag =1 表示数据带有头的标志 
int MyFec::encode_Data( unsigned char *buf, int len , int head_flag )
{		
	int esi;
	head_t  head;
	int total_num ;	 
	int num = 0;
	unsigned short flag;// = 0x0fff;   //分隔标示 0x0fff为没有分隔 
	
	int reallen = m_Config.pkg_len - sizeof(unsigned short);//实际的数据包内容长度
	int max_noenc_len = LAN_DATA - m_Config.pkg_len;
	int real_noenc_len;

	head.user_id = m_Config.userId;

	if (head_flag == TYPE_I) {
		m_isI = true;
	}

	//从缓冲区取残留的数据
	if( len + m_Send_Buflen< reallen )
	{
		memcpy(m_Send_Buf+m_Send_Buflen,buf,len);
		if (head_flag)
		{			
			if (m_Head_Flag == TYPE_SPLIT) 	m_Head_Flag = m_Send_Buflen;// set_headflag(head_flag, m_Send_Buflen);
		}
		m_Send_Buflen += len;
		return  1;
	}
	if (head_flag)
	{
		if (m_Head_Flag == TYPE_SPLIT) m_Head_Flag = m_Send_Buflen;// = flag = m_Head_Flag;
		//else flag = m_Send_Buflen;// set_headflag(head_flag, m_Send_Buflen);
	}
	//else
	//{
	//	if (m_Head_Flag != 0x0fff) flag = m_Head_Flag;
	//}

	if (m_isI) flag = set_headflag(TYPE_I, m_Head_Flag);
	else  flag = set_headflag(head_flag, m_Head_Flag);

	memcpy((unsigned char*)m_Enc_symbols_tab[m_Enc_Index], &flag, sizeof(flag));
	num += sizeof(flag);
	memcpy((unsigned char*)m_Enc_symbols_tab[m_Enc_Index]+num, m_Send_Buf, m_Send_Buflen );	
	num += m_Send_Buflen;
	memcpy((unsigned char*)m_Enc_symbols_tab[m_Enc_Index]+num, buf, reallen - m_Send_Buflen  );	

	head.packet_id = m_Seq++;
	memcpy( (unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index], &head, sizeof(head_t));
	memcpy( (unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index]+sizeof(head_t), (unsigned char*)m_Enc_symbols_tab[m_Enc_Index], m_Config.pkg_len );	
	//add no encode data
	if ( max_noenc_len > 0 )
	{
		unsigned char noenc_buf[SINGLE_NOENCBUF_LEN];
		real_noenc_len=get_send_noenc_buf(noenc_buf);
		if(real_noenc_len >0 )
			memcpy((unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index] + m_Config.pkg_len + sizeof(head_t), 
				noenc_buf, real_noenc_len);
		handle_single_encode_src((unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index], m_Config.pkg_len + sizeof(head_t)+ real_noenc_len, 1);
	}
	else
		handle_single_encode_src( (unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index], m_Config.pkg_len + sizeof(head_t), 1 );

	m_Head_Flag = TYPE_SPLIT;
	m_Enc_Index++;

	if( m_Enc_Index == m_Config.src_num ) //可以去生成检验包了
	{
		m_isI = false;
		if(create_Enc_Repair() < 0 ) 
		{
			OF_PRINT_LVL(1, ("ERROR: of_build_repair_symbol() failed \n"))
			m_Enc_Index = 0;
			return -1;
		}
		m_Enc_Index = 0;
	}	
	num = reallen - m_Send_Buflen ; //为构建第一个包用掉的Buf内容，num为目前的buf开始点
	m_Send_Buflen = 0; //buf 清0
	while( num < len )
	{
		if( len - num < reallen ) 
		{
			//OF_PRINT_LVL(2, ("packet lenfth is too short!,put in buf \n"))
			memcpy(m_Send_Buf+m_Send_Buflen,buf+num,len - num );
			m_Send_Buflen += len - num;			
			return  1;
		}

		if (m_isI) flag = set_headflag(TYPE_I, TYPE_SPLIT);
		else  flag = TYPE_SPLIT;

		//flag = 0x0fff;
		memcpy((unsigned char*)m_Enc_symbols_tab[m_Enc_Index], &flag, sizeof(flag));
		memcpy((unsigned char*)m_Enc_symbols_tab[m_Enc_Index] + sizeof(flag), buf+num, reallen );	
		num += reallen;

		head.packet_id = m_Seq++;
		memcpy( (unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index], &head, sizeof(head_t));

		memcpy( (unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index]+sizeof(head_t), (unsigned char*)m_Enc_symbols_tab[m_Enc_Index], m_Config.pkg_len );

		if (max_noenc_len > 0)
		{
			unsigned char noenc_buf[SINGLE_NOENCBUF_LEN];
			real_noenc_len = get_send_noenc_buf(noenc_buf);
			if (real_noenc_len >0)
				memcpy((unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index] + m_Config.pkg_len + sizeof(head_t),
					noenc_buf, real_noenc_len);
			handle_single_encode_src((unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index], m_Config.pkg_len + sizeof(head_t) + real_noenc_len, 1);
		}
		else
			handle_single_encode_src((unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index], m_Config.pkg_len + sizeof(head_t), 1);

		//handle_single_encode_src( (unsigned char*)m_Enc_send_symbols_tab[m_Enc_Index], m_Config.pkg_len + sizeof(head_t),1 );	
		m_Enc_Index++;

		if( m_Enc_Index == m_Config.src_num ) //可以去生成检验包了
		{
			m_isI = false;
			if(create_Enc_Repair() < 0 ) 
			{
				OF_PRINT_LVL(1, ("ERROR: of_build_repair_symbol() failed \n"))
				m_Enc_Index = 0;
				return  -1;
			}
			m_Enc_Index = 0;
		}
	}
	return 1;

}

void MyFec::handle_single_encode_src( unsigned char *buf, int len,int flag)
{
}

void MyFec::handle_single_decode_src( unsigned char *buf, int len, int flag)
{

}

void MyFec::handle_audio()
{

}

unsigned short  MyFec::set_headflag(int headflag, int len)
{
	unsigned short stemp1, stemp2;
	stemp1 = headflag;
	stemp2 = len;

	return (stemp1 << 12) | (stemp2 & 0x0fff);
}


int MyFec::decode_Data( unsigned char *buf, int len )
{
	head_t  head;
	int num = 0;
	int total_num = m_Config.src_num +m_Config.repair_num;	
	unsigned int mod;	
	int reallen = m_Config.pkg_len+ sizeof(head_t);
	int pkglen = m_Config.pkg_len;

	if (len < reallen) return 1;

	memcpy(&head, buf,sizeof(head_t));
	num += sizeof(head_t);	
	mod = head.packet_id % total_num;

	if (head.packet_id < m_Index && head.packet_id + m_Config.reset_num > m_Index )
	{
		// 此包已经超时
		if (mod < m_Config.src_num)
			OF_PRINT_LVL(2, ("src packet %d is timeout \n", head.packet_id))
		else
			OF_PRINT_LVL(2, ("repair packet %d is timeout \n", head.packet_id))
		return 1;
	}
	else 
	{
		if (head.packet_id >= m_Index + total_num)
		{
			OF_PRINT_LVL(1, ("packet %d >= %d \n", head.packet_id, m_Index + total_num))
			//新包已经到来，旧包将试着恢复，然后丢弃
			if (repair_DecBuf(mod, -2, pkglen) < 0)
			{
				OF_PRINT_LVL(1, ("packet %d is lost \n", m_Index))
				push_IncompleteBuf(pkglen);
				//m_Lost_flag = -1;
				m_Index = head.packet_id - head.packet_id % total_num;
				m_Dec_Num = 0;
				m_Pushed_seq = 0;
				memset(m_Hash_src, 0, sizeof(unsigned char)*total_num);
				m_err_num++;
			}
			else m_sta_num++;
		}
		else if (head.packet_id + m_Config.reset_num <= m_Index)
		{
			// 此时估计已经断线重连了，需要重置
			OF_PRINT_LVL(2, ("%d + %d <= %d, percent reset fec\n", head.packet_id, m_Config.reset_num, m_Index))
			m_Lost_flag = -1;
			m_Index = head.packet_id - head.packet_id % total_num;
			m_Dec_Num = 0;
			m_Pushed_seq = 0;
			memset(m_Hash_src, 0, sizeof(unsigned char)*total_num);
		}
		if (m_Hash_src[mod])
		{
			// 说明是断线重连了需要重置
			OF_PRINT_LVL(2, ("hash reset fec\n"))
			m_Lost_flag = -1;
			m_Index = head.packet_id - head.packet_id % total_num;
			m_Dec_Num = 0;
			m_Pushed_seq = 0;
			memset(m_Hash_src, 0, sizeof(unsigned char)*total_num);
		}

		memcpy(m_Dec_symbols_tab[mod], buf + num, pkglen);
		m_Hash_src[mod] = 1;
		m_Dec_Num++;
		// check 能不能把buf推给回调
		if (mod < m_Config.src_num) //判断是不是非检验包
		{

			//把不需编码的数据取出来
			if (len - reallen > 0)
			{
				memcpy(m_Noenc_Recv[mod].buf, buf + reallen, len - reallen);
				m_Noenc_Recv[mod].len = len - reallen;				
			}
			else
				m_Noenc_Recv[mod].len = 0;

			fill_recv_noenc_buf(m_Noenc_Recv[mod]);
			handle_audio();

			if(push_DecBuf(pkglen) >0) m_sta_num++; //得到可以推送的数据
		}
		if (m_Dec_Num == m_Config.src_num) //可以进行修复了
		{
			//printf("last id is %d\n", mod);
			//通过恢复，把余下的buf推给回调
			if( repair_DecBuf(mod,-1, pkglen) > 0) m_sta_num++;
		}
		else if (m_Dec_Num > m_Config.src_num)
		{
			//printf("last id is %d\n", mod);
			//通过恢复，把余下的buf推给回调
			if(repair_DecBuf(mod, 0, pkglen)>0) m_sta_num++;
		}
	}
	return 1;		
}

int MyFec::create_Enc_Repair()
{
	/* Now build the n-k repair symbols... */
	int ret;
	int esi;
	int total_num = m_Config.src_num +m_Config.repair_num;
	head_t  head;
	int max_noenc_len = LAN_DATA - m_Config.pkg_len;
	int real_noenc_len;

	head.user_id = m_Config.userId;
	
	for (esi =m_Config.src_num; esi < total_num; esi++)
	{
		if (of_build_repair_symbol(m_Encode_Ses, m_Enc_symbols_tab, esi) != OF_STATUS_OK) 
		{
			OF_PRINT_LVL(1, ("ERROR: of_build_repair_symbol() failed for esi=%u\n", esi))
			return  -1;
		}
		
		head.packet_id = m_Seq++;
		memcpy( (unsigned char*)m_Enc_send_symbols_tab[esi], &head, sizeof(head_t));
		memcpy( (unsigned char*)m_Enc_send_symbols_tab[esi]+sizeof(head_t), (unsigned char*)m_Enc_symbols_tab[esi], m_Config.pkg_len );
		/*if (max_noenc_len > 0)
		{
			unsigned char noenc_buf[LAN_DATA];
			real_noenc_len = get_send_noenc_buf(noenc_buf, max_noenc_len);
			if (real_noenc_len >0)
				memcpy((unsigned char*)m_Enc_send_symbols_tab[esi] + m_Config.pkg_len + sizeof(head_t),
					noenc_buf, real_noenc_len);
			handle_single_encode_src((unsigned char*)m_Enc_send_symbols_tab[esi], m_Config.pkg_len + sizeof(head_t) + real_noenc_len, 0);
		}
		else
			handle_single_encode_src((unsigned char*)m_Enc_send_symbols_tab[esi], m_Config.pkg_len + sizeof(head_t), 0);
			*/
		handle_single_encode_src( (unsigned char*)m_Enc_send_symbols_tab[esi], m_Config.pkg_len + sizeof(head_t),0 );			
	}
	return 1;
}

int MyFec::push_IncompleteBuf(int  len) {
	int i;
	int total_num = m_Config.src_num + m_Config.repair_num;	

	for (i = m_Pushed_seq; i < m_Config.src_num; i++)
	{
		if (!m_Hash_src[i]) 
		{
			m_Lost_flag = -1;
			continue;
		}
		//fill_recv_noenc_buf(m_Noenc_Recv[i]);
		if (m_Lost_flag < 0)
		{
					
			handle_single_decode_src((unsigned char*)m_Dec_symbols_tab[i], /*len*/m_Config.pkg_len, m_Lost_flag);
			m_Lost_flag = 0;
		}
		else
		{			
			handle_single_decode_src((unsigned char*)m_Dec_symbols_tab[i], /*len*/m_Config.pkg_len, 1);
		}
	}
	return 1;
}

int MyFec::push_DecBuf(int len  )
{
	int i;
	int total_num = m_Config.src_num + m_Config.repair_num;

	

	for( i =m_Pushed_seq; i < m_Config.src_num; i++ )
	{
		if( !m_Hash_src[i]) break;
		//fill_recv_noenc_buf(m_Noenc_Recv[i]);
		if( m_Lost_flag < 0 )
		{
			handle_single_decode_src((unsigned char*)m_Dec_symbols_tab[i], /*len*/m_Config.pkg_len, m_Lost_flag );
			m_Lost_flag = 0;
		}
		else
		{
			handle_single_decode_src((unsigned char*)m_Dec_symbols_tab[i], /*len*/m_Config.pkg_len, 0);
		}
	}
	m_Pushed_seq = i;
	
	// 若是全部把源包推送完了，就不需要修复了，直接算解码成功
	if( m_Pushed_seq == m_Config.src_num )
	{	
		m_Index = m_Index+ total_num;
		memset( m_Hash_src, 0, sizeof(unsigned char)*total_num);
		m_Dec_Num =0;
		m_Pushed_seq = 0;
		return 1;
	}
	return -1;
}



int MyFec::repair_DecBuf(int id, int flag ,int len)
{
	int i;
	int ret;
	

	void**      Dec_Src_tab =NULL;
	
	int done = 0;
	
	int total_num = m_Config.src_num + m_Config.repair_num;

	
	if( flag == -1 ) // first repair
	{
		if (m_Decode_Ses)
		{
			of_release_codec_instance(m_Decode_Ses);
			m_Decode_Ses = NULL;
		}
		if ((ret = of_create_codec_instance(&m_Decode_Ses, m_Config.codecId, OF_DECODER, of_verbosity)) != OF_STATUS_OK)
		{
			OF_PRINT_LVL(1, ("decode  of_create_codec_instance() failed\n"))
			return  -1;
		}

		//m_Params->encoding_symbol_length = len;
		if (of_set_fec_parameters(m_Decode_Ses, m_Params) != OF_STATUS_OK)
		{
			OF_PRINT_LVL(1, ("of_set_fec_parameters() failed for codec_id \n"))
			return -1;
		}		
		for (i = 0; i < total_num; i++)
		{			
			if (m_Hash_src[i])
			{
				if (of_decode_with_new_symbol(m_Decode_Ses, m_Dec_symbols_tab[i], i) == OF_STATUS_ERROR)
				{
					OF_PRINT_LVL(1, ("of_decode_with_new_symbol() failed\n"))
					return  -1;
				}
			}
		}
	}
	else if( flag == 0)//接着上次repair的地方，继续repair
	{
		if (!m_Decode_Ses)
		{
			OF_PRINT_LVL(1, (" decode_ses not exist\n"))
			return  -1;
		}
		if (of_decode_with_new_symbol(m_Decode_Ses, m_Dec_symbols_tab[id], id) == OF_STATUS_ERROR)
		{
			OF_PRINT_LVL(1, ("of_decode_with_new_symbol() failed\n"))
			return  -1;
		}
	}

	/* check if completed in case we received k packets or more */
	if ( flag != -2 )
	{
		if (of_is_decoding_complete(m_Decode_Ses) == true)
		{
			done = 1;
			OF_PRINT_LVL(1, (" num is %d  push is %d decoding_complete succed \n", m_Dec_Num, m_Pushed_seq))
		}
		else
		{
			OF_PRINT_LVL(1, (" num is %d  push is %d decoding_complete failed \n", m_Dec_Num, m_Pushed_seq))

		}
	}
	
	if (!done )
	{
		if( flag != -2 )
		{
			return -1;
		}

		if (!m_Decode_Ses) return -1;

		ret = of_finish_decoding(m_Decode_Ses );
		if (ret == OF_STATUS_OK)
		{
			OF_PRINT_LVL(1, ("num is %d  push is %d  of_finish_decoding() succed \n", m_Dec_Num, m_Pushed_seq))
		}
		else if (ret == OF_STATUS_ERROR || ret == OF_STATUS_FATAL_ERROR)
		{
			OF_PRINT_LVL(1, ("of_finish_decoding() failed with error \n"))
			if (m_Decode_Ses)
			{
				of_release_codec_instance(m_Decode_Ses);
				m_Decode_Ses = NULL;
			}
			return -1;
		}
		else  //meaning of_finish_decoding didn't manage to recover all source symbols
		{
			OF_PRINT_LVL(1, ("of_finish_decoding() failed with error \n"))
			if (m_Decode_Ses)
			{
				of_release_codec_instance(m_Decode_Ses);
				m_Decode_Ses = NULL;
			}
			return -1;			
		}
	}
	
	if ((Dec_Src_tab = (void**)calloc(total_num, sizeof(void*))) == NULL)
	{
		OF_PRINT_LVL(1, ("no memory (calloc failed for enc_send__symbols_tab, n=%u)\n", total_num))
		if (m_Decode_Ses)
		{
			of_release_codec_instance(m_Decode_Ses);
			m_Decode_Ses = NULL;
		}
		return  -1;
	}

	if (of_get_source_symbols_tab(m_Decode_Ses, Dec_Src_tab) != OF_STATUS_OK)
	{
		OF_PRINT_LVL(1, ("of_get_source_symbols_tab() failed\n"))
		if (m_Decode_Ses)
		{
			of_release_codec_instance(m_Decode_Ses);
			m_Decode_Ses = NULL;
		}
		free(Dec_Src_tab);
		return  -1;
	}
	
	for (i = m_Pushed_seq; i < m_Config.src_num; i++)
	{
		//if (m_Hash_src[i])
		//{
			//fill_recv_noenc_buf(m_Noenc_Recv[i]);
		//}
		if (m_Lost_flag < 0)
		{
			handle_single_decode_src((unsigned char*)Dec_Src_tab[i], /*len*/m_Config.pkg_len, m_Lost_flag);
			m_Lost_flag = 0;
		}
		else
			handle_single_decode_src((unsigned char*)Dec_Src_tab[i], /*len*/m_Config.pkg_len, 0);			
	}

	for (i = 0; i < total_num; i++)
	{
		if (!m_Hash_src[i])
		{
			free(Dec_Src_tab[i]);
		}
	}
	free(Dec_Src_tab);

	m_Index = m_Index + total_num;
	memset(m_Hash_src, 0, sizeof(unsigned char)* total_num);
	m_Dec_Num = 0;
	m_Pushed_seq = 0;
	if (m_Decode_Ses)
	{
		of_release_codec_instance(m_Decode_Ses);
		m_Decode_Ses = NULL;
	}
	return  1;
}

int MyFec::set_no_encdata(unsigned char *buf, int len)
{
	if (m_sendbuf_begin > MAX_NOENCBUF_LEN / 2)
	{
		if ( m_sendbuf_begin != m_sendbuf_end) 	
			memmove( m_Send_Noenc_Buf, m_Send_Noenc_Buf + m_sendbuf_begin, m_sendbuf_end - m_sendbuf_begin);
		m_sendbuf_end -= m_sendbuf_begin;
		m_sendbuf_begin = 0;
	}
	if (len > SINGLE_NOENCBUF_LEN)
	{
		//m_Set_Audi_Err_Num++;
		return  0;
	}
	if (m_sendbuf_end + len >= MAX_NOENCBUF_LEN)
	{
		//m_SendBuf_Small_Num++;
		return 0;
	}
	//OF_PRINT_LVL(1, (" set_no_encdata	%d \n", len))
	memcpy(m_Send_Noenc_Buf + m_sendbuf_end, buf, len);
	m_sendbuf_end += len;

	//m_Add_Buf_Len += len;
	//OF_PRINT_LVL(1, (" set_no_encdata	%d  %d\n", len, m_sendbuf_end, m_sendbuf_begin))
	return 1;
}

int MyFec::get_no_encdata(unsigned char *buf)
{
	int buflen;
	
	//OF_PRINT_LVL(1, (" get_no_encdata	%d %d  \n", m_recvbuf_end, m_recvbuf_begin))

	buflen = m_recvbuf_end - m_recvbuf_begin;
	memcpy(buf, m_Recv_Noenc_Buf + m_recvbuf_begin, buflen);
	m_recvbuf_end = 0;
	m_recvbuf_begin = 0;
	return buflen;	
}

int MyFec::get_send_noenc_buf( unsigned char* buf )
{
	int buflen;
	int offset;
	packet_head_t  head;
	int  total_len = 0;
	int  real_len = 0;

	buflen = m_sendbuf_end - m_sendbuf_begin;

	while ( buflen > 0 )
	{
		//OF_PRINT_LVL(1, (" get_send_noenc_buf	%d  %d \n", m_sendbuf_end, m_sendbuf_begin))
		if (buflen < sizeof(packet_head_t))
		{
			//m_ErrPacket_Num++;
			return real_len;
		}
		memcpy(&head, m_Send_Noenc_Buf + m_sendbuf_begin, sizeof(packet_head_t));
		//OF_PRINT_LVL(1, (" get_send_noenc_buf 111	%d  %d \n", head.get_size(), sizeof(packet_head_t)))
		offset = head.get_size() + sizeof(packet_head_t);

		//OF_PRINT_LVL(1, (" 22 get_send_noenc_buf	%d \n", real_len))
		if (buflen < offset) return real_len;
		total_len += offset;
		if (total_len > SINGLE_NOENCBUF_LEN) return real_len;
		memcpy(buf+ real_len, m_Send_Noenc_Buf + m_sendbuf_begin, offset);
		//m_Send_Buf_Len += offset;
		m_sendbuf_begin += offset;
		if (m_sendbuf_begin == m_sendbuf_end)
		{
			m_sendbuf_end = 0;
			m_sendbuf_begin = 0;
		}
		real_len += offset;
		buflen = m_sendbuf_end - m_sendbuf_begin;
	}
	//OF_PRINT_LVL(1, (" 11 get_send_noenc_buf	%d \n", real_len))
	return real_len;
}

int MyFec::fill_recv_noenc_buf(struct nofecbuf_t &noenc)
{
	//OF_PRINT_LVL(1, (" fill	%d \n", noenc.len))
	if (noenc.len <= 0) return 0;
	if(m_recvbuf_begin > MAX_NOENCBUF_LEN / 2)
	{
		if ( m_recvbuf_begin != m_recvbuf_end )
			memmove(m_Recv_Noenc_Buf, m_Recv_Noenc_Buf + m_recvbuf_begin, m_recvbuf_end - m_recvbuf_begin);
		m_recvbuf_end -= m_recvbuf_begin;
		m_recvbuf_begin = 0;
	}
	if (m_recvbuf_end + noenc.len >= MAX_NOENCBUF_LEN) return 0;
	memcpy(m_Recv_Noenc_Buf + m_recvbuf_end, noenc.buf, noenc.len);
	m_recvbuf_end += noenc.len;
	return 1;
}
