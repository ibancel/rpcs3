#pragma once

#include <array>

#include "utils.h"

#include "Utilities/File.h"

constexpr u32 SDAT_FLAG = 0x01000000;
constexpr u32 EDAT_COMPRESSED_FLAG = 0x00000001;
constexpr u32 EDAT_FLAG_0x02 = 0x00000002;
constexpr u32 EDAT_ENCRYPTED_KEY_FLAG = 0x00000008;
constexpr u32 EDAT_FLAG_0x10 = 0x00000010;
constexpr u32 EDAT_FLAG_0x20 = 0x00000020;
constexpr u32 EDAT_DEBUG_DATA_FLAG = 0x80000000;

struct loaded_npdrm_keys
{
	atomic_t<u128> devKlic{};
	atomic_t<u128> rifKey{};
	atomic_t<u32> npdrm_fds{0};
};

struct NPD_HEADER
{
	u32 magic;
	s32 version;
	s32 license;
	s32 type;
	char content_id[0x30];
	u8 digest[0x10];
	u8 title_hash[0x10];
	u8 dev_hash[0x10];
	u64 unk1;
	u64 unk2;
};

struct EDAT_HEADER
{
	s32 flags;
	s32 block_size;
	u64 file_size;
};

// Decrypts full file, or null/empty file
extern fs::file DecryptEDAT(const fs::file& input, const std::string& input_file_name, int mode, u8 *custom_klic, bool verbose);

extern bool VerifyEDATHeaderWithKLicense(const fs::file& input, const std::string& input_file_name, const u8* custom_klic, std::string* contentID);

u128 GetEdatRifKeyFromRapFile(const fs::file& rap_file);

struct EDATADecrypter final : fs::file_base
{
	// file stream
	const fs::file edata_file;
	u64 file_size{0};
	u32 total_blocks{0};
	u64 pos{0};

	NPD_HEADER npdHeader{};
	EDAT_HEADER edatHeader{};

	// Internal data buffers.
	std::unique_ptr<u8[]> data_buf{};
	u64 data_buf_size{0};

	u128 dec_key{};

	// edat usage
	u128 rif_key{};
	u128 dev_key{};
public:
	// SdataByFd usage
	EDATADecrypter(fs::file&& input)
		: edata_file(std::move(input)) {}
	// Edat usage
	EDATADecrypter(fs::file&& input, const u128& dev_key, const u128& rif_key)
		: edata_file(std::move(input))
		, rif_key(rif_key)
		, dev_key(dev_key) {}

	~EDATADecrypter() override {}
	// false if invalid
	bool ReadHeader();
	u64 ReadData(u64 pos, u8* data, u64 size);

	fs::stat_t stat() override
	{
		fs::stat_t stats = edata_file.stat();
		stats.is_writable = false; // TODO
		stats.size = file_size;
		return stats;
	}

	bool trunc(u64) override
	{
		return false;
	}

	u64 read(void* buffer, u64 size) override
	{
		const u64 bytesRead = ReadData(pos, static_cast<u8*>(buffer), size);
		pos += bytesRead;
		return bytesRead;
	}

	u64 write(const void*, u64) override
	{
		return 0;
	}

	u64 seek(s64 offset, fs::seek_mode whence) override
	{
		const s64 new_pos =
			whence == fs::seek_set ? offset :
			whence == fs::seek_cur ? offset + pos :
			whence == fs::seek_end ? offset + size() : -1;

		if (new_pos < 0)
		{
			fs::g_tls_error = fs::error::inval;
			return -1;
		}

		pos = new_pos;
		return pos;
	}

	u64 size() override { return file_size; }
};
