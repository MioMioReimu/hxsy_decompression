#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <zlib.h>
#include <errno.h>
#include <string>
#include <boost/filesystem.hpp>
#pragma pack(push,1)
struct pkg_fileinfo
{
	uint32_t unkown_1;
	uint32_t unkown_2;
	uint32_t offset;
	uint32_t unkown_3;
	uint32_t stream_size;  //the file compressed bytes
	uint32_t crc32; //every each file is not close. it should be crc32
	uint32_t unkown_5;//always 0x 00 00 00 00
	uint64_t unkown_6;	//always 0x 20 00 00 00 | 00 00 00 00 it should be file attributes;
	uint64_t create_time;	//this two time is unix time stamp . must be create time,modify time,
	uint64_t modify_time;  
	uint64_t unknown_7; //adjacent file 's value is very close but the subtraction is not stream_size or file_size.
	uint32_t file_size;	//the file's original size
	char filename[260];
	char dir[264];
	uint32_t idx; //which in the parted files
};
#pragma pack(pop)
#define CHUNK 262144
int read_idx(const char* dir, pkg_fileinfo **fileinfos);
int decompress_pkg_file(const char*dir, pkg_fileinfo *fi, unsigned char * buf, uint64_t buf_size,uint64_t *de_size);
int inflate_stream(const unsigned char *src, uint64_t src_size, unsigned char * dst, uint64_t dst_buffersize, uint64_t *de_size);
int read_idx(const char* dir,pkg_fileinfo **fileinfos)
{
	char path[256] = { 0 };
	FILE *f_idx = fopen(strcat(strcat(strcat(path, dir), "\\"), "pkg.idx"), "rb");
	assert(f_idx != NULL);
	fseek(f_idx, 0, SEEK_END);
	uint64_t fsize = ftell(f_idx);
	int file_num = (fsize - 288) / sizeof(pkg_fileinfo);
	*fileinfos = (pkg_fileinfo *)malloc(sizeof(pkg_fileinfo)*file_num);
	fseek(f_idx, 288, 0);
	uint64_t readed_size = 0;
	uint8_t * file_buffer = (uint8_t*)(*fileinfos);

	while (!feof(f_idx) || readed_size<file_num*sizeof(pkg_fileinfo))
	{
		uint64_t pos = ftell(f_idx);
		readed_size += fread(file_buffer + readed_size, 1, sizeof(pkg_fileinfo), f_idx);
	}
	fclose(f_idx);
	return file_num;
}
int decompress_pkg_file(const char*dir, pkg_fileinfo *fi, unsigned char * buf, uint64_t buf_size,uint64_t *de_size)
{
	char path[256] = { 0 };
	sprintf(path, "%s\\pkg%03d.pkg", dir, fi->idx);
	*de_size = 0;
	FILE *f = fopen(path, "rb");
	if (f == NULL)
	{
		return -1;
	}
	unsigned char *src_buffer = (uint8_t*)malloc(fi->stream_size);
	fseek(f, 0, SEEK_END);
	uint64_t file_size = ftell(f);
	fseek(f, fi->offset,SEEK_SET);
	fread(src_buffer, 1, fi->stream_size, f);

	//uLong crc_value = crc32(0L, Z_NULL, 0);
	//crc_value = crc32(crc_value, src_buffer, fi->stream_size);
	if (ferror(f)){
		free(src_buffer);
		fclose(f);
		return -1;
	}
	int ret = inflate_stream(src_buffer, fi->stream_size, buf, buf_size,de_size);
	if (ret != Z_OK){
		printf("%s%s decompress failed!\n",fi->dir, fi->filename);
		free(src_buffer);
		fclose(f);
		return -1;
	}
	free(src_buffer);
	fclose(f);
	return 0;
}
int inflate_stream(const unsigned char *src, uint64_t src_size, unsigned char * dst, uint64_t dst_buffersize, uint64_t *de_size)
{
	int ret;
	*de_size= 0;
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];
	
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
		return ret;

	do { 
		strm.avail_in = src_size;
		if (strm.avail_in == 0)
			break;
		strm.next_in = (Bytef*)src;
		do {
			strm.avail_out = dst_buffersize;
			strm.next_out = (Bytef*)dst;
			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);
			switch (ret)
			{
			case Z_NEED_DICT:
				printf("need dict\n");
				ret = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}
			*de_size += dst_buffersize - strm.avail_out;
		} while (strm.avail_out == 0);
	} while (ret != Z_STREAM_END);
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END?Z_OK:Z_DATA_ERROR;
}
void main(int argc,char **argv)
{
#define BUFSIZE 1024*1024*50
	char path[1024] = { 0 };
	pkg_fileinfo * file_infos;
	int file_num = read_idx(argv[1], &file_infos);
	unsigned char *buf = (uint8_t *)malloc(BUFSIZE);
	uint64_t fsize;
	int ret;
	FILE *f = NULL;
	
	for (int i = 0; i < file_num; ++i)
	{
		
		ret = decompress_pkg_file(argv[1], file_infos + i, buf, BUFSIZE, &fsize);
		if (ret == Z_OK)
		{
			sprintf(path, ".\\decps\\%s%s", file_infos[i].dir, file_infos[i].filename);
			if(!boost::filesystem::exists(".\\decps\\"+std::string(file_infos[i].dir)))
				boost::filesystem::create_directories(".\\decps\\" + std::string(file_infos[i].dir));
			//uLong crc_value = crc32(0L, Z_NULL, 0);
			//crc_value = crc32(crc_value, buf, fsize);
			ret = fopen_s(&f, path, "wb");
			assert(fsize == file_infos[i].file_size);
			fwrite(buf, 1, fsize, f);
			fclose(f);
		}
	}
}