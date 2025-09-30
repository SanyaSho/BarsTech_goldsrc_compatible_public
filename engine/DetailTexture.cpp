#include "quakedef.h"
#include "DetailTexture.h"
#include "gl_vidnt.h"
#include "qgl.h"

#include <string>
#include <vector>
#include <map>


struct DetailMap
{
	std::string _diffuseName;
	std::string _detailName;
	float _sScale;
	float _tScale;
	GLint _oglTextureId;
	GLint _oglDetailId;
};

enum ParseState
{
    PS_START,
    PS_DIFFUSE_NAME,
    PS_DETAIL_TEXTURE,
    PS_S_SCALE_FACTOR,
    PS_T_SCALE_FACTOR,
    PS_ENTRY,
    PS_ERROR,
    PS_COMPLETE
};

enum LexerState
{
    LS_START,
    LS_WHITE_SPACE,
    LS_COMMENT,
    LS_TOKEN,
    LS_ERROR,
    LS_COMPLETE
};

std::vector<DetailMap*> g_detailVector;
bool g_detTexLoaded;
std::string g_levelName;
bool g_demandLoad;
std::vector<std::pair<std::string, int>> g_decalTexIDs;
std::map<int, DetailMap*> g_idMap;
std::map<std::string, int> g_detailNameToIdMap;
std::map<std::string, int> g_detailLoadFailedMap;

bool detTexSupported = false;

cvar_t r_detailtextures = { const_cast<char*>("r_detailtextures"), const_cast<char*>("0"), FCVAR_ARCHIVE };
cvar_t r_detailtexturessupported = { const_cast<char*>("r_detailtexturessupported"), const_cast<char*>("1"), FCVAR_SPONLY };

GLint lastDetTex = -1;
GLfloat lastDetSScale = 1.0f;
GLfloat lastDetTScale = 1.0f;

GLint loadTextureFile(char* filename)
{
	byte* data;
	int txID;
	char newFileName[4096];
	int width, height;

	data = (byte*)malloc(1024 * 1024);
	snprintf(newFileName, sizeof(newFileName), "gfx/%s.tga", filename);

	if (data == nullptr)
		return -1;
	
	txID = -1;
	
	if (LoadTGA2(newFileName, data, 1024 * 1024, &width, &height, 0))
		txID = GL_LoadTexture2(filename, GLT_SPRITE, width, height, data, true, TEX_TYPE_RGBA, nullptr, GL_LINEAR_MIPMAP_LINEAR);
	
	free(data);

	return txID;
}

void DT_Initialize()
{
	Cvar_RegisterVariable(&r_detailtextures);
	Cvar_RegisterVariable(&r_detailtexturessupported);

	qglSelectTextureSGIS(TEXTURE2_SGIS);
	qglEnable(GL_TEXTURE_2D);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	qglTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0);
	qglDisable(GL_TEXTURE_2D);
	qglSelectTextureSGIS(TEXTURE0_SGIS);

	detTexSupported = true;
}

bool DT_GetToken(char** const cPtr, ParseState* const parseState, std::string* const token)
{
	char *sPtr = nullptr, *ePtr = nullptr;
	char ch;
	LexerState state = LS_START;

	while (state != LS_COMPLETE && state != LS_ERROR)
	{
		switch (state)
		{
		case LS_START:
			sPtr = *cPtr;
			state = LS_WHITE_SPACE;
			ePtr = sPtr;
			break;
		case LS_WHITE_SPACE:
			while (ch = *sPtr, (ch == '\r' || ch == '\n' || ch == '\t' || ch == ' '))
				sPtr++;

			ePtr = sPtr;

			if (ch == -1)
			{
				state = LS_COMPLETE;
				*parseState = PS_COMPLETE;
			}
			else
			{
				state = LS_COMMENT;
			}
			break;
		case LS_COMMENT:
			state = LS_TOKEN;

			if (sPtr[0] == '/' && sPtr[1] == '/')
			{
				sPtr += 2;
				if (*sPtr != '\r' && *sPtr != '\n')
				{
					while (*sPtr != -1)
					{
						sPtr++;
						if (*sPtr == '\n') break;
						if (*sPtr == '\r') break;
					}
				}
				state = LS_WHITE_SPACE;
			}
			break;
		case LS_TOKEN:
			const char special[] = "_.-/{~!+[]";

			while (true)
			{
				if (
					(*ePtr - 'A') > 'Z' - 'A' &&
					(*ePtr - 'a') > 'z' - 'a' &&
					(*ePtr - '0') > '9' - '0'
					)
				{
					bool invalid = false;
					for (int i = 0; i < sizeof(special) - 1; i++)
					{
						if (*ePtr == special[i])
						{
							invalid = true;
							break;
						}
					}
					if (invalid)
						break;
				}
				ePtr++;
			}

			if (ePtr == sPtr)
			{
				state = LS_ERROR;
				*parseState = PS_ERROR;
				break;
			}

			token->assign(sPtr, ePtr - sPtr);

			if (token->empty())
				state = LS_ERROR;

			break;
		}
	}


	*cPtr = ePtr;

	return (*parseState != PS_COMPLETE && *parseState != PS_ERROR);
}

void DT_Parse(char* pBuffer)
{
	std::string diffuseName;
	std::string detailName;
	std::string scaleFactorString;
	ParseState state[4];
	char* cPtr[4];
	float sScaleFactor;
	float tScaleFactor;
	DetailMap* pMap = nullptr;

	ParseState curState = PS_START;

	state[0] = curState;

	while (curState != PS_COMPLETE && curState != PS_ERROR)
	{
		switch (curState)
		{
		case PS_START:
			curState = PS_DIFFUSE_NAME;
			cPtr[0] = pBuffer;
			state[0] = curState;
			break;
		case PS_DIFFUSE_NAME:
			if (!DT_GetToken(cPtr, state, &diffuseName))
			{
				curState = state[0];
				if (curState == PS_COMPLETE || curState == PS_ERROR)
				{
					if (curState == PS_ERROR)
						Con_Printf(const_cast<char*>("Error parsing detail texture file.\n"));

					return;
				}
			}
			curState = PS_DETAIL_TEXTURE;
			state[0] = curState;
			break;
		case PS_DETAIL_TEXTURE:
			if (!DT_GetToken(cPtr, state, &detailName))
			{
				curState = state[0];
				if (curState == PS_COMPLETE || curState == PS_ERROR)
				{
					if (curState == PS_ERROR)
						Con_Printf(const_cast<char*>("Error parsing detail texture file.\n"));

					return;
				}
			}
			curState = PS_S_SCALE_FACTOR;
			state[0] = curState;
			break;
		case PS_S_SCALE_FACTOR:
			if (!DT_GetToken(cPtr, state, &scaleFactorString))
			{
				curState = state[0];
				if (curState == PS_COMPLETE || curState == PS_ERROR)
				{
					if (curState == PS_ERROR)
						Con_Printf(const_cast<char*>("Error parsing detail texture file.\n"));

					return;
				}
			}

			sScaleFactor = atof(scaleFactorString.c_str());

			curState = PS_T_SCALE_FACTOR;
			state[0] = curState;
			break;
		case PS_T_SCALE_FACTOR:
			if (!DT_GetToken(cPtr, state, &scaleFactorString))
			{
				curState = state[0];
				if (curState == PS_COMPLETE || curState == PS_ERROR)
				{
					if (curState == PS_ERROR)
						Con_Printf(const_cast<char*>("Error parsing detail texture file.\n"));

					return;
				}
			}

			tScaleFactor = atof(scaleFactorString.c_str());

			curState = PS_ENTRY;
			state[0] = curState;
			break;
		case PS_ENTRY:
			pMap = new DetailMap;

			pMap->_diffuseName = diffuseName;
			pMap->_detailName = detailName;
			pMap->_oglDetailId = pMap->_oglTextureId = 0;
			pMap->_sScale = sScaleFactor;
			pMap->_tScale = tScaleFactor;

			g_detailVector.push_back(pMap);

			curState = PS_DIFFUSE_NAME;
			state[0] = curState;
			break;
		}
	}
}

const char* DT_GetDetailTextureName(char* diffuseName)
{
	static char buffer[128];

	for (int idx = 0; idx < g_detailVector.size(); idx++)
	{
		if (!stricmp(g_detailVector[idx]->_diffuseName.c_str(), diffuseName))
		{
			std::string detailName = g_detailVector[idx]->_detailName;

			if (!detailName.empty())
			{
				strcpy(buffer, detailName.c_str());
				return buffer;
			}
		}
	}

	return nullptr;
}

void DT_SetTextureCoordinates(float u, float v)
{
	qglMTexCoord2fSGIS(TEXTURE2_SGIS, u, v);
}

void DT_ClearRenderState()
{
	qglSelectTextureSGIS(TEXTURE2_SGIS);
	qglDisable(GL_TEXTURE_2D);
	qglSelectTextureSGIS(TEXTURE1_SGIS);
}

void DT_LoadDetailMapFile(char* levelName)
{
	if (!detTexSupported)
		return;

	if (!r_detailtextures.value)
	{
		g_detTexLoaded = false;
		g_levelName.assign(levelName, strlen(levelName));
		return;
	}

	g_detTexLoaded = true;

	if (!g_demandLoad)
		g_decalTexIDs.clear();
	
	int count = g_detailVector.size();

	for (int idx = 0; idx < count; idx++)
	{
		if (g_detailVector[idx] != nullptr)
		{
			delete g_detailVector[idx];
		}
		g_detailVector[idx] = nullptr;
	}
	g_detailVector.clear();

	g_idMap.clear();
	g_detailNameToIdMap.clear();
	g_detailLoadFailedMap.clear();

	std::string fileName = "maps/" + std::string(levelName) + "_detail.txt";

	FileHandle_t file = FS_Open(fileName.c_str(), "rb");

	if (!file)
	{
		Con_Printf(const_cast<char*>("No detail texture mapping file: %s\n"), fileName.c_str());
		return;
	}

	int fileSize = FS_FileSize(fileName.c_str());
	
	char* pBuffer = new char[fileSize + 10];

	if (FS_Read(pBuffer, fileSize, 1, file) <= 0)
	{
		if (pBuffer)
			delete[] pBuffer;

		Con_Printf(const_cast<char*>("Detail texture mapping file read failed\n"));
		return;
	}

	FS_Close(file);

	pBuffer[fileSize] = -1;

	DT_Parse(pBuffer);

	if (pBuffer)
		delete[] pBuffer;
}

void DT_LoadDetailTexture(char* diffuseName, int diffuseId)
{
	std::string detailName;
	std::string filename;

	if (!detTexSupported)
		return;

	if (!r_detailtextures.value)
	{
		std::string strDiffuseName = diffuseName;
	//	g_decalTexIDs.emplace_back(std::make_pair(strDiffuseName, diffuseId));
		g_decalTexIDs.assign( { std::make_pair(strDiffuseName, diffuseId) } );
		return;
	}

	for (int idx = 0; idx < g_detailVector.size(); idx++)
	{
		if (!stricmp(g_detailVector[idx]->_diffuseName.c_str(), diffuseName))
		{
			detailName = g_detailVector[idx]->_detailName;

			if (g_detailLoadFailedMap.find(detailName) != g_detailLoadFailedMap.end())
				return;

			if (g_detailNameToIdMap.find(detailName) == g_detailNameToIdMap.end())
			{
				int detailId;

				detailId = loadTextureFile(const_cast<char*>(filename.c_str()));

				if (detailId == -1)
				{
					Con_Printf(const_cast<char*>("Detail texture map load failed: %s\n"), detailName.c_str());
					g_detailLoadFailedMap[detailName] = 0;
					continue;
				}

				g_detailNameToIdMap[detailName] = detailId;
				g_detailVector[idx]->_oglDetailId = detailId;
			}
		}
	}
}

int DT_SetRenderState(int diffuseId)
{
	if (!detTexSupported || !r_detailtexturessupported.value)
		return 0;

	if (!g_detTexLoaded)
	{
		Con_Printf(const_cast<char*>("Loading Detail Textures...\n"));
		g_demandLoad = true;
		DT_LoadDetailMapFile(const_cast<char*>(g_levelName.c_str()));
		g_detTexLoaded = true;
		g_demandLoad = false;

		for (int i = 0; i < g_decalTexIDs.size(); i++)
		{
			auto p = g_decalTexIDs[i];
			DT_LoadDetailTexture(const_cast<char*>(p.first.c_str()), p.second);
		}
	}

	auto iter = g_idMap.find(diffuseId);

	if (iter != g_idMap.end())
	{
		DetailMap* pDetail = iter->second;
		if (pDetail->_oglDetailId != 0 && pDetail->_oglTextureId != 0)
		{
			qglSelectTextureSGIS(TEXTURE2_SGIS);
			qglEnable(GL_TEXTURE_2D);

			bool bind;

			if (lastDetTex != -1 && lastDetTex == pDetail->_oglDetailId)
			{
				bind = false;
			}
			else
			{
				lastDetTex = pDetail->_oglDetailId;
				bind = true;
			}

			bool scale = false;

			if (pDetail->_sScale != lastDetSScale || pDetail->_tScale != lastDetTScale)
			{
				scale = true;

				lastDetSScale = pDetail->_sScale;
				lastDetTScale = pDetail->_tScale;
			}

			if (bind)
				qglBindTexture(GL_TEXTURE_2D, pDetail->_oglDetailId);

			if (scale)
			{
				qglMatrixMode(GL_TEXTURE);
				qglLoadIdentity();
				qglScalef(pDetail->_sScale, pDetail->_tScale, 1.0);
				qglMatrixMode(GL_MODELVIEW);
			}

			return true;
		}
	}

	return false;
}
