#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "hash.h"
#include "ecc.h"
#define N0 16  //hash开头有N0个2进制0 
#define blockBytes 816 //每个区块所占字节数 
#define transBytes 80 //每笔交易记录所占字节数
#define MaxNum 10000 //试nonce1,nonce2的最大数 

typedef struct {
	BYTE from[33];//264 bits 转出公钥 
	BYTE to[33];//264 bits 转入公钥 
	double amount;//64 bits 转账的虚拟币数量
	BYTE signature[64];//512 ECC数字签名,使用from的私钥，进行数字签名，这个是签名结果  
}Transaction; 

typedef struct BLOCK {
	BYTE prev_hash[32];//256 bits 上一个数据块的杂凑计算结果
	long long chain_version;//64 bits 可以用来表示版本号，以及区块ID，和别人的区块链区分开
	long long nonce1;//64 bits
	long long nonce2;//64 bits
	double award;//64 bits 奖励 
	BYTE coinbase[32];//256 bits 挖矿者公钥
	Transaction trans[5];//n笔交易打包为一个区块  
	struct BLOCK *next;//(4bytes) 由于struct对齐，占8bytes  
} BLOCK;//824 bytes 

typedef unsigned char BYTE;             
void struct2str(BYTE *p, BYTE *s, int structBytes)
//区块结构体转为字符数组 用于计算hash 
//输入参数时把结构体指针转为字符指针 structBytes为结构体所占字节数,可小于实际结构体容量，即部分转换 
//每次运行字符数组会被更新 
{
	int i;
	for (i = 0; i < structBytes; i++)
		s[i] = *((BYTE *)p + i);
}
void long2str(long long n, BYTE *s)//long long 64bits 分成8个BYTE 
{
	int i;
	long long a = n;
	for (i = 0; i < 8; i++)
		s[i] = *((BYTE *)&a + i);
}
void Block_init(BLOCK *p, BYTE *hash)
{
	int i, n;
	int N = 5;//创建N条交易记录 
	BYTE str[transBytes + 1];//用于存储交易记录 构成字符串生成hash 
	SHA256_CTX ctx;

	p->chain_version = 121;
	for (i = 0; i < 32; i++)//记录前一个hash 
		p->prev_hash[i] = hash[i];
	p->nonce1 = 0;
	p->nonce2 = 0;
	for (i = 0; i < 32; i++)
		p->coinbase[i] = rand() % 256;//0-255随机数 
	p->award = 134;

	for (n = 0; n < N; n++)//创建N条交易记录 
	{
		BYTE privateKey_f[32], privateKey_t[32], publicKey_f[33], publicKey_t[33], trans_hash[32], signature[64];
		Transaction trans;//临时存储交易记录 
		for (i = 0; i < 32; i++)//随机生成私钥 
		{
			privateKey_f[i] = rand() % 256;
			privateKey_t[i] = rand() % 256;
		}
		ecc_make_key(publicKey_f, privateKey_f);//ecc生成公钥 
		ecc_make_key(publicKey_t, privateKey_t);

		trans.amount = rand() % 10000;//随机生成交易金额 
		for (i = 0; i < 33; i++)//记录公钥 
		{
			trans.from[i] = publicKey_f[i];
			trans.to[i] = publicKey_t[i];
		}
		//printf("sizeof trans:%d\n",sizeof(trans));
		struct2str((BYTE *)&trans, str, transBytes); //交易记录转字符串 
		sha256_init(&ctx);
		sha256_update(&ctx, str, transBytes);
		sha256_final(&ctx, trans_hash);//生成交易记录hash 
		if (n == 4)//第五笔交易 
		{
			privateKey_f[3] += 1;//修改私钥 模仿假的私钥 验证数字签名应该不通过 
		}
		ecdsa_sign(privateKey_f, trans_hash, signature);//使用from的私钥对交易记录进行数字签名 
		if (ecdsa_verify(publicKey_f, trans_hash, signature))
		{
			printf("transaction %d verified\n", n + 1);
			for (i = 0; i < 64; i++)//记录数字签名 
			{
				trans.signature[i] = signature[i];
			}
			p->trans[n] = trans;//存在区块中 
		}
		else
			printf("transaction %d verification failed\n", n + 1);

	}
}
int Mining(BLOCK *p, BYTE *hash, int n0)
//找到nonce1 nonce2 使得block的hash满足开头有n0个2进制0 
//试不同nonce，区块转为字符数组 验证verify 存下最终nonce, hash 
{
	long long i, j;
	int v = 0;
	BYTE str[blockBytes];
	struct2str((BYTE *)p, str, blockBytes);
	for (i = 0; v == 0 && i < MaxNum; i++)
	{
		long2str(i, &str[40]);//直接修改字符数组内的值，避免重新struct2str 提高效率 
		for (j = 0; v == 0 && j < MaxNum; j++)
		{
			long2str(j, &str[48]);
			v = verify(str, hash, n0);
		}
	}
	if (v)//满足要求
	{
		p->nonce1 = i;
		p->nonce2 = j;
		/*
		int k;
		for (k = 0; k < 96; k++)
			printf("%d ", str[k]);
		printf("\n");验证字符串数组*/
		return 1;
	}
	return 0;
}
int verify(BYTE *str, BYTE *hash, int n)
// 字符串数组str 使得hash前n个2进制数为0 满足返回1 
{
	int i, m, r;
	SHA256_CTX ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, str, blockBytes);//strlen 遇到0截断 
	sha256_final(&ctx, hash);
	m = n / 8;//0占BYTE个数 
	r = n % 8;//余数 
	for (i = 0; i < m; i++)
	{
		if (hash[i] != 0)
			return 0;
	}
	switch (r)//hash前面m个BYTE都为0时 判断第m+1个BYTE是否前面含r个0
	{
	case 0: return 1;
	case 1: if (hash[i] <= 0x7f) return 1; break;
	case 2: if (hash[i] <= 0x3f) return 1; break;
	case 3: if (hash[i] <= 0x1f) return 1; break;
	case 4: if (hash[i] <= 0x0f) return 1; break;
	case 5: if (hash[i] <= 0x07) return 1; break;
	case 6: if (hash[i] <= 0x03) return 1; break;
	case 7: if (hash[i] <= 0x01) return 1; break;
	}
	return 0;
}

void print_block(BLOCK *p)
{
	int j, n;
	int N = 5;
	puts("previous hash:");
	for (j = 0; j < 32; j++)
		printf("%x ", p->prev_hash[j]);
	printf("\nnonce1:%lld nonce2:%lld\n", p->nonce1, p->nonce2);//注意lld才能正确显示long long 
	/*
	for(n=0;n<N;n++)
	{
		printf("\n%d th transation signature:",n+1);
		for(j=0;j<64;j++)
			printf("%x ",p->trans[n].signature[j]);
	}
	*/
}
void writefile(BLOCK *block, int num);
int main() {
	int i, j, N = 10;
	unsigned char hash[32] = { 0 }; //256 bits hash  0用于给第一个区块赋值 
	BLOCK *pre, *h;//前区块指针 区块链头指针 
	for (i = 0; i < N; i++)//生成10个区块 
	{
		BLOCK *block = (BLOCK *)malloc(sizeof(BLOCK));//分配的内存跳出循环仍存在 
		puts("creating new block:\n");
		Block_init(block, hash);//创建初始区块 hash为上一个区块的 第一个区块hash=0 
		Mining(block, hash, N0);
		//printf("sizeof block:%d size of transaction:%d\n",sizeof(*block),sizeof(block->trans));
		printf("\ninfo of %d th block:\n", i + 1);
		print_block(block);
		printf("hash:");
		for (j = 0; j < 32; j++)
			printf("%x ", hash[j]);
		puts("\n");
		writefile(block, i + 1);

		if (i == 0)//第一个区块 
		{
			h = block;//记录链头地址 
			pre = block;
			continue; //跳过下面的 
		}
		pre->next = block;//新区块加入链，上一个区块记录新区块地址 
		pre = block;//用最新的区块地址更新pre
	}
	//printf("%lld",h->next->next->nonce2);//验证链建好 第三个block的nonce2 
	pre->next = NULL;

	//malloc free
	return 0;
	system("pause");
}
void writefile(BLOCK *block, int num)//num为块序号 
{
	char s[] = "blockchain_data/block";
	char n[4];
	itoa(num, n, 10);
	strcat(s, n);
	FILE *fp = fopen(s, "wb");
	if (fp == NULL)
	{
		printf("can't open the file\n");
		exit(0);
	}
	fwrite(block, blockBytes, 1, fp);//每个区块信息存储到二进制文件 block->next不存，无意义 	
	//blockBytes=sizeof(BLOCK)-8  block->next(4bytes) 由于struct对齐，占8 
	fclose(fp);
}
