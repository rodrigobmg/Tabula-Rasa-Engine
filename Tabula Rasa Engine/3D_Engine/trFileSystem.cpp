#include "trApp.h"
#include "trFileSystem.h"
#include "trLog.h"
#include "PhysFS/include/physfs.h"

#pragma comment( lib, "PhysFS/libx86/physfs.lib" )

trFileSystem::trFileSystem()
{
	this->name = "FileSystem";
}

trFileSystem::~trFileSystem() { RELEASE(assets_dir); }

bool trFileSystem::Awake(JSON_Object * config)
{
	// Initializing PhysFS library
	TR_LOG("trFileSystem: initializing PhysFS library...\n");

	bool ret = true;

	if (PHYSFS_init(nullptr) != 0)
		TR_LOG("trFileSystem: success on initializing PhysFS \n");
	else
	{
		ret = false;
		TR_LOG("trFileSystem: error initializing PhysFS: %s \n", PHYSFS_getLastError());
	}	

	TR_LOG("trFileSystem: setting search directories...\n");

	// Setting search directories
	AddNewPath(".");
	AddNewPath("../Game");

	// Setting game directory to write in it
	PHYSFS_setWriteDir(".");

	assets_dir = new Directory(ASSETS_DIR);
	return true;
}

bool trFileSystem::Start()
{
	return true;
}

bool trFileSystem::CleanUp()
{
	bool ret = false;

	TR_LOG("trFileSystem: deinitializing PHYSFS...\n");

	if (PHYSFS_deinit() != 0)
	{
		ret = true;
		TR_LOG("trFileSystem: PhysFs successfully deinitialized...\n");
	}
	else
		TR_LOG("trFileSystem: error while deinitaliazing PHYSFS: %s\n", PHYSFS_getLastError());

	return true;
}

bool trFileSystem::DoesFileExist(const char * file_name) const
{
	bool ret = false;

	if (PHYSFS_exists(file_name) != 0)
		ret = true;

	return ret;
}

bool trFileSystem::DoesDirExist(const char * dir_name) const
{
	bool ret = true;

	if (PHYSFS_isDirectory(dir_name) != 0)
		TR_LOG("trFileSystem: directory %s found\n", dir_name);
	else
	{
		ret = false;
		TR_LOG("trFileSystem: directory %s not found\n", dir_name);
	}

	return ret;
}

void trFileSystem::ClearAssetsDir(Directory* dir_to_clean)
{
	dir_to_clean->files_vec.clear();

	for (uint i = 0u; i < dir_to_clean->dirs_vec.size(); i++)
	{
		ClearAssetsDir(&dir_to_clean->dirs_vec[i]);
	}

	dir_to_clean->dirs_vec.clear();
}

void trFileSystem::RefreshDirectory(const char* dir_name)
{
	std::string assets_name(dir_name);
	assets_name.append("/");
	char **rc = PHYSFS_enumerateFiles(assets_name.c_str());

	if (rc == nullptr)
	{
		TR_LOG("Directory %s not found", dir_name);
		return;
	}
	else
	{
		std::string directory = assets_name.c_str();

		for (char** i = rc; *i != nullptr; i++)
		{
			if (DoesDirExist((directory + *i).c_str()))
			{
				std::string tmp_dir = assets_name;
				tmp_dir.append(*i);

				Directory new_dir(*i);
				assets_dir->dirs_vec.push_back(new_dir);

				RefreshDirectory(tmp_dir.c_str());
			
				assets_index++;
			}
			else
				assets_dir->dirs_vec[assets_index].files_vec.push_back(*i);
		}
	} 
	PHYSFS_freeList(rc);

}


bool trFileSystem::AddNewPath(const char * path)
{
	bool ret = false;

	if (PHYSFS_mount(path, nullptr, 1) != 0)
	{
		ret = true;
		TR_LOG("trFileSystem: archive/dir successfully added to the search path.\n");
	}
	else
		TR_LOG("trFileSystem: could not add archive/dir to the search path: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));

	return ret;
}

PHYSFS_File* trFileSystem::OpenFileForWriting(const char * file_name) const
{
	PHYSFS_File* file = PHYSFS_openWrite(file_name);

	if (file != nullptr)
		TR_LOG("trFileSystem: file %s successfully opened for writing.\n", file_name);
	else
		TR_LOG("trFileSystem: could not open file %s for writting: %s\n", file_name, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));

	return file;
}

PHYSFS_File* trFileSystem::OpenFileForReading(const char* file_name) const
{
	PHYSFS_File* file = PHYSFS_openRead(file_name);

	if (file != nullptr)
		TR_LOG("trFileSystem: file %s successfully opened for reading.\n", file_name);
	else
		TR_LOG("trFileSystem: could not open file %s for reading: %s\n", file_name, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));

	return file;
}

void trFileSystem::CloseFile(PHYSFS_File* file, const char* file_name) const
{
	if (PHYSFS_close(file) != 0)
		TR_LOG("trFileSystem: success on closing file %s\n", file_name);
	else
		TR_LOG("trFileSystem: could not close file %s: %s\n", file_name, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
}

bool trFileSystem::WriteInFile(const char* file_name, char* buffer, uint size) const
{
	bool ret = true;

	PHYSFS_File* file = OpenFileForWriting(file_name);

	if (file == nullptr || PHYSFS_writeBytes(file, (const void*)buffer, size) < size)
	{
		ret = false;
		TR_LOG("trFileSystem: could not write to file %s: %s\n", file_name, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
	}
	else
		TR_LOG("trFileSystem: success on writting to file %s\n", file_name);

	CloseFile(file, file_name);

	return ret;
}

uint trFileSystem::ReadFromFile(const char* file_name, char** buffer)
{
	uint size = 0u;
	PHYSFS_File* file = OpenFileForReading(file_name);

	if (file != nullptr)
	{
		size = PHYSFS_fileLength(file);

		*buffer = new char[size];

		if (PHYSFS_readBytes(file, *buffer, size) == -1)
		{
			size = 0u;
			TR_LOG("trFileSystem: could not read from file %s: %s\n", file_name, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
		}
		else
			TR_LOG("trFileSystem: success on reading from file %s: %s\n", file_name, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
	}

	CloseFile(file, file_name);

	return size;
}

void trFileSystem::GetExtensionFromFile(const char * file_name, std::string & extension)
{
	extension = file_name;

	uint point = extension.find_last_of(".");
	if (point != std::string::npos)
		extension = extension.substr(point);
}

bool trFileSystem::MakeNewDir(const char * dir_name)
{
	bool ret = true;

	if (PHYSFS_mkdir(dir_name) != 0)
		TR_LOG("trFilesystem: success on creating directory %s\n", dir_name);
	else
	{
		ret = false;
		TR_LOG("trFilesystem: could not create directory %s: %s\n", dir_name, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
	}

	return ret;
}

bool trFileSystem::DeleteFileDir(const char* file_dir_name)
{
	bool ret = true;

	if (PHYSFS_delete(file_dir_name) != 0)
		TR_LOG("trFilesystem: success on deleting %s\n", file_dir_name);
	else
	{
		ret = false;
		TR_LOG("trFilesystem: could not delete %s: %s\n", file_dir_name, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
	}

	return ret;
}

Directory* trFileSystem::GetAssetsDirectory() const
{
	return assets_dir;
}