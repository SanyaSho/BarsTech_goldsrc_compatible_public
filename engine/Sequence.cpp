#include "quakedef.h"
#include "Sequence.h"
#include "pr_cmds.h"

unsigned int g_nonGlobalSentences;
qboolean g_sequenceParseFileIsGlobal;
sentenceGroupEntry_s *g_sentenceGroupList;
sequenceEntry_s *g_sequenceList;
char *g_lineScan, *g_scan;
char g_sequenceParseFileName[256];
int g_lineNum;
sequenceCommandLine_s g_blockScopeDefaults, g_fileScopeDefaults;

const sequenceCommandMapping_s g_sequenceCommandMappingTable[] =
{
	{ SEQUENCE_COMMAND_PAUSE, "pause", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_TEXT, "text", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_SOUND, "sound", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_FIRETARGETS, "firetargets", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_KILLTARGETS, "killtargets", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_GOSUB, "gosub", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_SENTENCE, "sentence", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_REPEAT, "repeat", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_SETDEFAULTS, "setdefaults", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_MODIFIER, "modifier", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_POSTMODIFIER, "postmodifier", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_COMMAND_NOOP, "noop", SEQUENCE_TYPE_COMMAND },
	{ SEQUENCE_MODIFIER_EFFECT, "effect", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_POSITION, "position", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_COLOR, "color", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_COLOR2, "color2", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_FADEIN, "fadein", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_FADEOUT, "fadeout", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_HOLDTIME, "holdtime", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_FXTIME, "fxtime", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_SPEAKER, "speaker", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_LISTENER, "listener", SEQUENCE_TYPE_MODIFIER },
	{ SEQUENCE_MODIFIER_TEXTCHANNEL, "channel", SEQUENCE_TYPE_MODIFIER }
};

sentenceEntry_s* SequenceGetSentenceByIndex(unsigned int index)
{
	sentenceEntry_s* se;
	
	if (g_sentenceGroupList == NULL)
		return NULL;

	unsigned int numSentences = g_sentenceGroupList->numSentences;

	if (index < numSentences)
	{
		for (numSentences = 0, se = g_sentenceGroupList->firstSentence; index > numSentences; se = se->nextEntry, numSentences++);
		return se;
	}
	else
	{
		for (sentenceGroupEntry_s* list = g_sentenceGroupList->nextEntry; list != NULL; list = list->nextEntry)
		{
			if (numSentences + list->numSentences > index)
			{
				for (se = g_sentenceGroupList->firstSentence; index > numSentences; se = se->nextEntry, numSentences++);
				return se;
			}
			numSentences += list->numSentences;
		}
	}

	return NULL;
}

void Sequence_StripComments(char *buffer, int *pBufSize)
{
	char *end;
	char *pch;
	char *edit;
	char control;

	end = &buffer[*pBufSize];
	pch = edit = buffer;

	while (pch < end)
	{
		if (!*pch)
			break;

		if (*pch == '/')
		{
			if (pch[1] == '/')
			{
				while (*pch && (*pch != '\n' && *pch != '\r'))
					pch++;
				continue;
			}

			if (pch[1] == '*')
			{
				pch += 2;
				if (*pch)
				{
					while (pch[1])
					{
						if (*pch == '*' && pch[1] == '/')
							break;

						if (*pch == '\n' || *pch == '\r')
							*edit++ = *pch;

						if (!*++pch)
							break;
					}
				}
				continue;
			}
		}

		*edit++ = *pch++;
	}

	*edit = '\0';
}

void Sequence_ResetDefaults(sequenceCommandLine_s *destination, sequenceCommandLine_s *source)
{
	if (source)
	{
		destination->clientMessage.effect = source->clientMessage.effect;
		destination->clientMessage.r1 = source->clientMessage.r1;
		destination->clientMessage.g1 = source->clientMessage.g1;
		destination->clientMessage.b1 = source->clientMessage.b1;
		destination->clientMessage.a1 = source->clientMessage.a1;
		destination->clientMessage.r2 = source->clientMessage.r2;
		destination->clientMessage.g2 = source->clientMessage.g2;
		destination->clientMessage.b2 = source->clientMessage.b2;
		destination->clientMessage.a2 = source->clientMessage.a2;
		destination->clientMessage.x = source->clientMessage.x;
		destination->clientMessage.y = source->clientMessage.y;
		destination->clientMessage.fadein = source->clientMessage.fadein;
		destination->clientMessage.fadeout = source->clientMessage.fadeout;
		destination->clientMessage.holdtime = source->clientMessage.holdtime;
		destination->clientMessage.pName = 0;
		destination->clientMessage.pMessage = 0;
		destination->clientMessage.fxtime = source->clientMessage.fxtime;
		destination->textChannel = source->textChannel;
		destination->delay = source->delay;
		destination->nextCommandLine = 0;
		destination->soundFileName = 0;
		destination->repeatCount = source->repeatCount;
		if (destination->speakerName)
			Mem_Free(destination->speakerName);
		destination->speakerName = Mem_Strdup(source->speakerName);
		if (destination->listenerName)
			Mem_Free(destination->listenerName);
		destination->listenerName = Mem_Strdup(source->listenerName);
	}
	else
	{
		destination->clientMessage.effect = 0;
		destination->clientMessage.r1 = -1;
		destination->clientMessage.g1 = -1;
		destination->clientMessage.b1 = -1;
		destination->clientMessage.a1 = -1;
		destination->clientMessage.r2 = -1;
		destination->clientMessage.g2 = -1;
		destination->clientMessage.b2 = -1;
		destination->clientMessage.a2 = -1;
		destination->clientMessage.x = 0.5;
		destination->clientMessage.y = 0.5;
		destination->clientMessage.fadein = 0.2;
		destination->clientMessage.fadeout = 0.2;
		destination->clientMessage.holdtime = 1.6;
		destination->clientMessage.fxtime = 1.0;
		destination->clientMessage.pName = 0;
		destination->clientMessage.pMessage = 0;
		destination->textChannel = 0;
		destination->delay = 0.0;
		destination->repeatCount = 0;
		destination->nextCommandLine = 0;
		destination->soundFileName = 0;
		destination->speakerName = 0;
		destination->listenerName = 0;
	}
}

qboolean Sequence_SkipWhitespace()
{
	qboolean found = false;

	while (isspace(*g_scan))
	{
		if (*g_scan == '\n')
		{
			g_lineScan = &g_scan[1];
			g_lineNum++;
			found = true;
		}

		g_scan++;
	}

	return found;
}

char Sequence_GetSymbol()
{
	Sequence_SkipWhitespace();

	if (*g_scan)
		return *g_scan++;
	else
		return *g_scan;
}

void Sequence_GetNameValueString(char *token, int tokenLen)
{
	int i;

	if (tokenLen <= 0)
		return;

	Sequence_SkipWhitespace();

	if (!isalnum(*g_scan))
	{
		if (*g_scan != '#' && *g_scan != '$')
			Sys_Error("Parsing error on line %d of %s.seq: expected name/value, found illegal character '%c'\n", g_lineNum, g_sequenceParseFileName, *g_scan);
		
		Sys_Error("Parsing error on line %d of %s.seq: cannot have more than one '%c' per line; '%c' must be at the beginning of the line ONLY\n",
			g_lineNum, g_sequenceParseFileName, *g_scan, *g_scan);
	}

	if (isalnum(*g_scan))
	{
		i = 0;
		do
		{
			if (i++ >= tokenLen - 1)
				break;

			*token++ = *g_scan;
		} while (isalnum(*++g_scan));
	}
	*token = 0;
}

sequenceCommandEnum_e Sequence_GetCommandEnumForName(const char *commandName)
{
	for (int i = 0; i < ARRAYSIZE(g_sequenceCommandMappingTable); i++)
	{
		if (!Q_strcasecmp(commandName, g_sequenceCommandMappingTable[i].commandName))
			return g_sequenceCommandMappingTable[i].commandEnum;
	}

	return SEQUENCE_COMMAND_ERROR;
}

qboolean Sequence_IsCommandAModifier(sequenceCommandEnum_e commandEnum)
{
	for (int i = 0; i < ARRAYSIZE(g_sequenceCommandMappingTable); i++)
	{
		if (g_sequenceCommandMappingTable[i].commandType == SEQUENCE_TYPE_MODIFIER)
			return true;
	}

	Sys_Error("Internal error caused by line %d of %s.seq: unknown command enum = %d\n",
		g_lineNum, g_sequenceParseFileName, commandEnum);
}

qboolean Sequence_ConfirmCarriageReturnOrSymbol(char symbol)
{
	return Sequence_SkipWhitespace() || *g_scan == symbol;
}

float Sequence_ReadFloat()
{
	char floatString[256];

	Sequence_SkipWhitespace();
	Sequence_GetNameValueString(floatString, sizeof(floatString));
	return atof(floatString);
}

int Sequence_ReadInt()
{
	char intString[256];

	Sequence_SkipWhitespace();
	Sequence_GetNameValueString(intString, sizeof(intString));
	return atol(intString);
}

void Sequence_ReadString(char *string, int stringLen)
{
	Sequence_SkipWhitespace();
	Sequence_GetNameValueString(string, stringLen);
}

void Sequence_ReadQuotedString(char *string, int stringLen)
{
	char sym;
	int i;
	char *in, *out;

	if (stringLen <= 0)
		return;

	Sequence_SkipWhitespace();
	sym = Sequence_GetSymbol();
	if (sym != '"')
		Sys_Error("Parsing error on or before line %d of %s.seq: expected quote (\"), found '%c' instead\n",
		g_lineNum, g_sequenceParseFileName, sym);

	in = g_scan;
	out = string;

	for (i = 0; *in && *in != '"' && i < stringLen - 1; i++, *out++ = *in++)
	{
		if (*in == '\n')
			g_lineNum++;
	}

	*out = '\0';
	g_scan = &in[1];
}

void Sequence_ReadCommandData(sequenceCommandEnum_e commandEnum, sequenceCommandLine_s *defaults)
{
	char *error, **endptr;
	char tempString[1024];
	char token[1024];

	switch (commandEnum)
	{
	case SEQUENCE_COMMAND_PAUSE:
		defaults->delay = Sequence_ReadFloat();
		break;
	case SEQUENCE_COMMAND_FIRETARGETS:
		Sequence_ReadQuotedString(tempString, sizeof(tempString));
		defaults->fireTargetNames = Mem_Strdup(tempString);
		break;
	case SEQUENCE_COMMAND_KILLTARGETS:
		Sequence_ReadQuotedString(tempString, sizeof(tempString));
		defaults->killTargetNames = Mem_Strdup(tempString);
		break;
	case SEQUENCE_COMMAND_TEXT:
		Sequence_ReadQuotedString(tempString, sizeof(tempString));
		defaults->clientMessage.pMessage = Mem_Strdup(tempString);
		break;
	case SEQUENCE_COMMAND_SOUND:
		Sequence_ReadString(tempString, sizeof(tempString));
		defaults->soundFileName = Mem_Strdup(tempString);
		break;
	case SEQUENCE_COMMAND_GOSUB:
		Sequence_ReadString(tempString, sizeof(tempString));
		defaults->clientMessage.pName = Mem_Strdup(tempString);
		break;
	case SEQUENCE_COMMAND_SENTENCE:
		Sequence_ReadString(tempString, sizeof(tempString));
		defaults->sentenceName = Mem_Strdup(tempString);
		break;
	case SEQUENCE_COMMAND_REPEAT:
		defaults->repeatCount = Sequence_ReadInt();
		break;
	case SEQUENCE_MODIFIER_EFFECT:
		defaults->modifierBitField |= SEQUENCE_MODIFIER_EFFECT_BIT;
		defaults->clientMessage.effect = Sequence_ReadInt();
		break;
	case SEQUENCE_MODIFIER_POSITION:
		defaults->clientMessage.x = Sequence_ReadFloat();
		defaults->clientMessage.y = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_POSITION_BIT;
		break;
	case SEQUENCE_MODIFIER_COLOR:
		defaults->clientMessage.r1 = Sequence_ReadInt();
		defaults->clientMessage.g1 = Sequence_ReadInt();
		defaults->clientMessage.b1 = Sequence_ReadInt();
		defaults->clientMessage.a1 = 255;
		defaults->modifierBitField |= SEQUENCE_MODIFIER_COLOR_BIT;
		break;
	case SEQUENCE_MODIFIER_COLOR2:
		defaults->clientMessage.r2 = Sequence_ReadInt();
		defaults->clientMessage.g2 = Sequence_ReadInt();
		defaults->clientMessage.b2 = Sequence_ReadInt();
		defaults->clientMessage.a2 = 255;
		defaults->modifierBitField |= SEQUENCE_MODIFIER_COLOR2_BIT;
		break;
	case SEQUENCE_MODIFIER_FADEIN:
		defaults->clientMessage.fadein = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_FADEIN_BIT;
		break;
	case SEQUENCE_MODIFIER_FADEOUT:
		defaults->clientMessage.fadein = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_FADEOUT_BIT;
		break;
	case SEQUENCE_MODIFIER_HOLDTIME:
		defaults->clientMessage.holdtime = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_HOLDTIME_BIT;
		break;
	case SEQUENCE_MODIFIER_FXTIME:
		defaults->clientMessage.fxtime = Sequence_ReadFloat();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_FXTIME_BIT;
		break;
	case SEQUENCE_MODIFIER_SPEAKER:
		Sequence_ReadString(tempString, sizeof(tempString));
		defaults->speakerName = Mem_Strdup(tempString);
		defaults->modifierBitField |= SEQUENCE_MODIFIER_SPEAKER_BIT;
		break;
	case SEQUENCE_MODIFIER_LISTENER:
		Sequence_ReadString(tempString, sizeof(tempString));
		defaults->listenerName = Mem_Strdup(tempString);
		defaults->modifierBitField |= SEQUENCE_MODIFIER_LISTENER_BIT;
		break;
	case SEQUENCE_MODIFIER_TEXTCHANNEL:
		defaults->textChannel = Sequence_ReadInt();
		defaults->modifierBitField |= SEQUENCE_MODIFIER_TEXTCHANNEL_BIT;
		break;
	default:
		Sys_Error("Internal error caused by line %d of %s.seq: unknown command enum = %d\n",
			g_lineNum, g_sequenceParseFileName, commandEnum);
	}
}

char Sequence_ParseModifier(sequenceCommandLine_s *defaults)
{
	char modifierName[256], sym;
	sequenceCommandEnum_e cmd;

	Sequence_GetNameValueString(modifierName, sizeof(modifierName));
	cmd = Sequence_GetCommandEnumForName(modifierName);

	if (cmd == SEQUENCE_COMMAND_ERROR)
		Sys_Error("Parsing error on line %d of %s.seq: unknown modifier \"%s\"\n",
		g_lineNum, g_sequenceParseFileName, modifierName);

	if (!Sequence_IsCommandAModifier(cmd))
		Sys_Error("Parsing error on line %d of %s.seq: \"%s\" is a #command, not a $modifier\n",
		g_lineNum, g_sequenceParseFileName, modifierName);

	sym = Sequence_GetSymbol();

	if (sym != '=')
		Sys_Error("Parsing error on or after line %d of %s.seq: after modifier \"%s\", expected '=', found '%c'\n",
		g_lineNum, g_sequenceParseFileName, modifierName, sym);

	Sequence_ReadCommandData(cmd, defaults);

	if (!Sequence_ConfirmCarriageReturnOrSymbol(','))
		Sys_Error("Parsing error on line %d of %s.seq: after value(s) for modifier \"%s\", expected ',' or End-Of-Line; found '%c'\n",
		g_lineNum, g_sequenceParseFileName, modifierName, *g_scan);

	return Sequence_GetSymbol();
}

char Sequence_ParseModifierLine(sequenceEntry_s *entry, int modifierType)
{
	sequenceCommandLine_s *defaults = NULL, *firstCommand;
	char modch;

	while (1)
	{
		if (modifierType == 1)
		{
			defaults = (sequenceCommandLine_s *)Mem_ZeroMalloc(sizeof(sequenceCommandLine_s));
			defaults->commandType = SEQUENCE_COMMAND_MODIFIER;
			firstCommand = entry->firstCommand;
			if (firstCommand)
			{
				while (firstCommand->nextCommandLine)
					firstCommand = firstCommand->nextCommandLine;
				firstCommand->nextCommandLine = defaults;
				defaults->nextCommandLine = 0;
			}
			else
			{
				entry->firstCommand = defaults;
				defaults->nextCommandLine = 0;
			}
		}

		if (modifierType == 0 || modifierType == 1)
		{
			modch = Sequence_ParseModifier(modifierType == 0 ? &g_fileScopeDefaults : defaults);
			if (modch != ',')
				return modch;
		}
	}
}

void Sequence_AddCommandLineToEntry(sequenceCommandLine_s *commandLine, sequenceEntry_s *entry)
{
	sequenceCommandLine_s *pcmd;

	pcmd = entry->firstCommand;
	if (pcmd)
	{
		while (pcmd->nextCommandLine)
			pcmd = pcmd->nextCommandLine;
		pcmd->nextCommandLine = commandLine;
		commandLine->nextCommandLine = NULL;
	}
	else
	{
		entry->firstCommand = commandLine;
		commandLine->nextCommandLine = NULL;
	}
}

void Sequence_CreateDefaultsCommand(sequenceEntry_s *entry)
{
	sequenceCommandLine_s *pcmd;

	pcmd = (sequenceCommandLine_s*)Mem_ZeroMalloc(sizeof(sequenceCommandLine_s));

	Sequence_ResetDefaults(pcmd, &g_fileScopeDefaults);

	pcmd->commandType = SEQUENCE_COMMAND_SETDEFAULTS;
	pcmd->modifierBitField = 255;

	Sequence_AddCommandLineToEntry(pcmd, entry);
}

char Sequence_ParseCommand(sequenceCommandLine_s *newCommandLine)
{
	char commandName[256], sym;
	sequenceCommandEnum_e cmd;
	sequenceCommandLine_s *pcmd, *plist;

	Sequence_GetNameValueString(commandName, sizeof(commandName));
	cmd = Sequence_GetCommandEnumForName(commandName);
	if (cmd == -1)
		Sys_Error("Parsing error on line %d of %s.seq: unknown command \"%s\"\n",
		g_lineNum, g_sequenceParseFileName, commandName);

	if (Sequence_IsCommandAModifier(cmd))
	{
		pcmd = (sequenceCommandLine_s *)Mem_ZeroMalloc(sizeof(sequenceCommandLine_s));
		plist = newCommandLine;
		pcmd->commandType = SEQUENCE_COMMAND_POSTMODIFIER;
		while (plist->nextCommandLine)
			plist = plist->nextCommandLine;
		plist->nextCommandLine = pcmd;
		newCommandLine = pcmd;
	}

	sym = Sequence_GetSymbol();
	if (sym != '=')
		Sys_Error("Parsing error on or before line %d of %s.seq: after command \"%s\", expected '=', found '%c'\n",
		g_lineNum, g_sequenceParseFileName, commandName, sym);
	Sequence_ReadCommandData(cmd, newCommandLine);
	return Sequence_GetSymbol();
}

char Sequence_ParseCommandLine(sequenceEntry_s *entry)
{
	sequenceCommandLine_s *pcmd;
	char sym;

	pcmd = (sequenceCommandLine_s *)Mem_ZeroMalloc(sizeof(sequenceCommandLine_s));
	Sequence_ResetDefaults(pcmd, &g_blockScopeDefaults);
	Sequence_AddCommandLineToEntry(pcmd, entry);
	while ((sym = Sequence_ParseCommand(pcmd)) == ',')
		;
	return sym;
}

char Sequence_ParseMacro(sequenceEntry_s *entry)
{
	sequenceCommandLine_s *pcmd;
	char sym;

	pcmd = (sequenceCommandLine_s *)Mem_ZeroMalloc(sizeof(sequenceCommandLine_s));
	Q_memset(pcmd, 0, sizeof(sequenceCommandLine_s));
	Sequence_ResetDefaults(pcmd, &g_blockScopeDefaults);
	Sequence_AddCommandLineToEntry(pcmd, entry);
	Sequence_ReadCommandData(SEQUENCE_COMMAND_GOSUB, pcmd);
	for (sym = Sequence_GetSymbol(); sym == ','; sym = Sequence_ParseCommand(pcmd));

	return sym;
}

char Sequence_ParseLine(char startToken, sequenceEntry_s *entry)
{
	if (startToken == '$')
		return Sequence_ParseModifierLine(entry, 1);
	if (startToken == '#')
		return Sequence_ParseCommandLine(entry);
	if (startToken != '@')
		Sys_Error("Parsing error on line %d of %s.seq: line must begin with either '#' (command) or '$' (modifier); found '%c'\n",
		g_lineNum, g_sequenceParseFileName, startToken);

	return Sequence_ParseMacro(entry);
}

qboolean Sequence_IsEntrySafe(sequenceEntry_s *entry)
{
	sequenceCommandLine_s *pcmd;
	float total_delay;

	if (!entry->firstCommand)
		return true;

	pcmd = entry->firstCommand;
	total_delay = .0f;
	do
	{
		total_delay += pcmd->delay;
		if (pcmd->repeatCount < 0 && total_delay <= 0)
			return false;
	} while ((pcmd = pcmd->nextCommandLine) != NULL);

	return true;
}

char Sequence_ParseEntry()
{
	char token[256], sym, cTok;
	sequenceEntry_s *pseq;

	Sequence_GetNameValueString(token, sizeof(token));

	sym = Sequence_GetSymbol();
	
	if (sym != '{')
		Sys_Error("Parsing error on line %d of %s.seq: expected '{' to start a\n new entry block; found '%c' instead!",
		g_lineNum, g_sequenceParseFileName, sym);

	pseq = (sequenceEntry_s*)Mem_Malloc(sizeof(sequenceEntry_s));
	pseq->entryName = Mem_Strdup(token);
	pseq->fileName = Mem_Strdup(g_sequenceParseFileName);
	pseq->firstCommand = NULL;
	pseq->isGlobal = g_sequenceParseFileIsGlobal;

	Sequence_CreateDefaultsCommand(pseq);

	for (cTok = Sequence_GetSymbol(); cTok != '!'; cTok = Sequence_ParseLine(cTok, pseq));

	if (!Sequence_IsEntrySafe(pseq))
		Sys_Error("Logic error in file %s.seq before line %d: execution of entry \"%%%s\" would cause an infinite loop!",
		g_sequenceParseFileName, g_lineNum, pseq->entryName);

	pseq->nextEntry = g_sequenceList;
	g_sequenceList = pseq;
	return Sequence_GetSymbol();
}

qboolean Sequence_IsSymbol(char ch)
{
	switch (ch)
	{
	case '"':
	case '#':
	case '$':
	case '%':
	case ',':
	case '=':
	case '@':
	case '{':
	case '}':
		return true;
	default:
		return false;
	}
}

void Sequence_GetToken(char *token, int tokenLen)
{
	Sequence_SkipWhitespace();
	if (isalnum(*g_scan))
	{
		Sequence_GetNameValueString(token, tokenLen);
	}
	else
	{
		if (!Sequence_IsSymbol(*g_scan))
			Sys_Error("Parsing error on line %d of %s.seq: expected token, found '%c' instead\n",
			g_lineNum, g_sequenceParseFileName, *g_scan);

		_snprintf(token, tokenLen, "%c", *g_scan++);
	}
}

void Sequence_GetLine(char *line, int lineMaxLen)
{
	char *pch;
	int len;

	Sequence_SkipWhitespace();
	pch = strchr(g_scan, '\n');
	if (!pch)
		Sys_Error("Syntax Error on line %d of %s.seq: expected sentence definition or '}', found End-Of-File!\n",
		g_lineNum, g_sequenceParseFileName);

	len = pch - g_scan;
	if (pch - g_scan >= lineMaxLen)
		Sys_Error("Syntax Error on line %d of %s.seq: line was too long (was %d chars; max is %d chars)\n",
		g_lineNum, g_sequenceParseFileName, pch - g_scan, lineMaxLen - 1);
	
	strncpy(line, g_scan, pch - g_scan);
	g_scan = pch;
	line[len] = 0;
}

sentenceGroupEntry_s *Sequence_FindSentenceGroup(const char *groupName)
{
	for (sentenceGroupEntry_s *pgroup = g_sentenceGroupList; pgroup != NULL; pgroup = pgroup->nextEntry)
	{
		if (!Q_strcasecmp(pgroup->groupName, groupName))
			return pgroup;
	}

	return NULL;
}

sentenceGroupEntry_s *Sequence_AddSentenceGroup(char *groupName)
{
	sentenceGroupEntry_s *plist;
	sentenceGroupEntry_s *pgroup;

	plist = g_sentenceGroupList;
	pgroup = (sentenceGroupEntry_s *)Mem_Malloc(sizeof(sentenceGroupEntry_s));
	pgroup->numSentences = 0;
	pgroup->firstSentence = 0;
	pgroup->nextEntry = 0;
	pgroup->groupName = Mem_Strdup(groupName);

	if (plist)
	{
		while (plist->nextEntry)
			plist = plist->nextEntry;
		plist->nextEntry = pgroup;
	}
	else
		g_sentenceGroupList = pgroup;

	return pgroup;
}

void Sequence_AddSentenceToGroup(char *groupName, char *data)
{
	sentenceGroupEntry_s *pgroup;
	sentenceEntry_s *pentry, *plist;

	pgroup = Sequence_FindSentenceGroup(groupName);
	if (!pgroup)
	{
		// попытка создать запись соответствующую имени, если таковая не найдена в списке
		pgroup = Sequence_AddSentenceGroup(groupName);
		if (!pgroup)
			Sys_Error("Unable to allocate sentence group %s at line %d in file %s.seq",
			groupName, g_lineNum, g_sequenceParseFileName);
	}

	pentry = (sentenceEntry_s *)Mem_Malloc(sizeof(sentenceEntry_s));
	pentry->nextEntry = 0;
	pentry->data = Mem_Strdup(data);
	pentry->isGlobal = g_sequenceParseFileIsGlobal;
	pentry->index = g_nonGlobalSentences++;
	pgroup->numSentences++;

	plist = pgroup->firstSentence;
	if (plist)
	{
		while (plist->nextEntry)
			plist = plist->nextEntry;
		plist->nextEntry = pentry;
	}
	else
		pgroup->firstSentence = pentry;
}

qboolean Sequence_ParseSentenceLine()
{
	char data[1024];
	char fullgroup[64];
	char groupName[64];
	char *start, *term;
	int i;

	Sequence_GetToken(fullgroup, sizeof(fullgroup));

	if (fullgroup[0] == '}')
		return 1;

	start = &fullgroup[strlen(fullgroup) - 1];
	while (!isalpha(*start) && *start != '_')
		start--;

	if (&start[1] && start[1])
		start[1] = 0;

	i = 0;
	do
	{
		groupName[i] = fullgroup[i];
	} while (groupName[i++]);

	Sequence_GetLine(data, sizeof(data));
	
	term = &data[strlen(data) - 1];
	if (*term == '\n' || *term == '\r')
		*term = '\0';

	Sequence_AddSentenceToGroup(groupName, data);

	return false;
}

char Sequence_ParseSentenceBlock()
{
	char sym;

	sym = Sequence_GetSymbol();
	if (sym != '{')
		Sys_Error("Parsing error on line %d of %s.seq: expected '{' to start a\n new sentence block; found '%c' instead!",
		g_lineNum, g_sequenceParseFileName, sym);

	while (!Sequence_ParseSentenceLine())
		;

	return Sequence_GetSymbol();
}

char Sequence_ParseGlobalDataBlock()
{
	char token[256];
	Sequence_GetNameValueString(token, sizeof(token));

	if (Q_strcasecmp(token, "Sentences"))
		Sys_Error("Syntax error in file %s.seq on line %d: found global data block symbol '!' with unknown data type \"%s\"",
		g_sequenceParseFileName, g_lineNum, token);

	return Sequence_ParseSentenceBlock();
}

sequenceEntry_s *Sequence_GetEntryForName(const char *entryName)
{
	for (sequenceEntry_s *pentry = g_sequenceList; pentry != NULL; pentry = pentry->nextEntry)
	{
		if (!Q_strcasecmp(entryName, pentry->entryName))
			return pentry;
	}
	return NULL;
}

sequenceCommandLine_s *Sequence_CopyCommand(sequenceCommandLine_s *commandOrig)
{
	sequenceCommandLine_s *pcmd;

	pcmd = (sequenceCommandLine_s *)Mem_ZeroMalloc(sizeof(sequenceCommandLine_s));
	pcmd->nextCommandLine = 0;
	pcmd->commandType = commandOrig->commandType;
	pcmd->modifierBitField = commandOrig->modifierBitField;
	pcmd->repeatCount = commandOrig->repeatCount;
	pcmd->delay = commandOrig->delay;
	pcmd->textChannel = commandOrig->textChannel;
	pcmd->fireTargetNames = Mem_Strdup(commandOrig->fireTargetNames);
	pcmd->killTargetNames = Mem_Strdup(commandOrig->killTargetNames);
	pcmd->listenerName = Mem_Strdup(commandOrig->listenerName);
	pcmd->sentenceName = Mem_Strdup(commandOrig->sentenceName);
	pcmd->soundFileName = Mem_Strdup(commandOrig->soundFileName);
	pcmd->speakerName = Mem_Strdup(commandOrig->speakerName);
	pcmd->clientMessage.effect = commandOrig->clientMessage.effect;
	pcmd->clientMessage.a1 = commandOrig->clientMessage.a1;
	pcmd->clientMessage.b1 = commandOrig->clientMessage.b1;
	pcmd->clientMessage.a2 = commandOrig->clientMessage.a2;
	pcmd->clientMessage.b2 = commandOrig->clientMessage.b2;
	pcmd->clientMessage.fadein = commandOrig->clientMessage.fadein;
	pcmd->clientMessage.fadeout = commandOrig->clientMessage.fadeout;
	pcmd->clientMessage.fxtime = commandOrig->clientMessage.fxtime;
	pcmd->clientMessage.g1 = commandOrig->clientMessage.g1;
	pcmd->clientMessage.g2 = commandOrig->clientMessage.g2;
	pcmd->clientMessage.holdtime = commandOrig->clientMessage.holdtime;
	pcmd->clientMessage.r1 = commandOrig->clientMessage.r1;
	pcmd->clientMessage.r2 = commandOrig->clientMessage.r2;
	pcmd->clientMessage.x = commandOrig->clientMessage.x;
	pcmd->clientMessage.y = commandOrig->clientMessage.y;
	pcmd->clientMessage.pMessage = Mem_Strdup(commandOrig->clientMessage.pMessage);
	pcmd->clientMessage.pName = Mem_Strdup(commandOrig->clientMessage.pName);
	return pcmd;
}

sequenceCommandLine_s *Sequence_CopyCommandList(sequenceCommandLine_s *list)
{
	sequenceCommandLine_s *pcmd, *last = NULL;

	for (sequenceCommandLine_s *commandOrig = list, *pcmdcache = NULL; commandOrig != NULL; commandOrig->nextCommandLine)
	{
		if (commandOrig->commandType != SEQUENCE_COMMAND_SETDEFAULTS)
		{
			pcmd = Sequence_CopyCommand(commandOrig);
			if (pcmdcache)
				pcmdcache->nextCommandLine = pcmd;
			else
				last = pcmd;
			pcmdcache = pcmd;
		}
	}

	return last;
}

qboolean Sequence_ExpandGosubsForEntry(sequenceEntry_s *entry)
{
	sequenceEntry_s *pentry;
	sequenceCommandLine_s *plist, *pdup;
	qboolean found = false;

	for (sequenceCommandLine_s *pcmd = entry->firstCommand; pcmd != NULL; pcmd = pcmd->nextCommandLine)
	{
		if (!pcmd->clientMessage.pName)
			continue;

		if (!Q_strcasecmp(pcmd->clientMessage.pName, entry->entryName))
			Sys_Error("Error in %s.seq: entry \"%s\" gosubs itself!\n", entry->fileName, entry->entryName);

		pentry = Sequence_GetEntryForName(pcmd->clientMessage.pName);
		if (!pentry)
			Sys_Error("Error in %s.seq: Gosub in entry \"%s\" specified unknown entry \"%s\"\n",
			entry->fileName, entry->entryName, pcmd->clientMessage.pName);

		found = true;
		pdup = Sequence_CopyCommandList(entry->firstCommand);
		if (pdup)
		{
			for (plist = pdup; plist->nextCommandLine != NULL; plist = plist->nextCommandLine);
			
			plist->nextCommandLine = entry->firstCommand->nextCommandLine;
			pentry->firstCommand->nextCommandLine = pdup;

			Mem_Free((void *)entry->firstCommand->clientMessage.pName);
			entry->firstCommand->clientMessage.pName = NULL;

			pcmd = plist;
		}
		else
		{
			Mem_Free((void*)pcmd->clientMessage.pName);
			pcmd->clientMessage.pName = NULL;
		}
	}

	return !found;
}

void Sequence_ExpandAllGosubs()
{
	sequenceEntry_s *entry;
	int term;

	do
	{
		entry = g_sequenceList;

		if (!entry)
			break;

		term = 0;
		do
		{
			if (!Sequence_ExpandGosubsForEntry(entry))
				term = 1;
		} while ((entry = entry->nextEntry) != NULL);
	} while (term);
}

void Sequence_WriteDefaults(sequenceCommandLine_s *source, sequenceCommandLine_s *destination)
{
	if (!destination)
		Sys_Error("Attempt to bake defaults into a non-existant command.");
	if (!source)
		Sys_Error("Attempt to bake defaults from a non-existant command.");

	if (source->modifierBitField & SEQUENCE_MODIFIER_EFFECT_BIT)
		destination->clientMessage.effect = source->clientMessage.effect;

	if (source->modifierBitField & SEQUENCE_MODIFIER_COLOR_BIT)
	{
		destination->clientMessage.r1 = source->clientMessage.r1;
		destination->clientMessage.g1 = source->clientMessage.g1;
		destination->clientMessage.b1 = source->clientMessage.b1;
		destination->clientMessage.a1 = source->clientMessage.a1;
	}

	if (source->modifierBitField & SEQUENCE_MODIFIER_COLOR2_BIT)
	{
		destination->clientMessage.r2 = source->clientMessage.r2;
		destination->clientMessage.g2 = source->clientMessage.g2;
		destination->clientMessage.b2 = source->clientMessage.b2;
		destination->clientMessage.a2 = source->clientMessage.a2;
	}

	if (source->modifierBitField & SEQUENCE_MODIFIER_POSITION_BIT)
	{
		destination->clientMessage.x = source->clientMessage.x;
		destination->clientMessage.y = source->clientMessage.y;
	}

	if (source->modifierBitField & SEQUENCE_MODIFIER_FADEIN_BIT)
		destination->clientMessage.fadein = source->clientMessage.fadein;
	
	if (source->modifierBitField & SEQUENCE_MODIFIER_FADEOUT_BIT)
		destination->clientMessage.fadeout = source->clientMessage.fadeout;

	if (source->modifierBitField & SEQUENCE_MODIFIER_HOLDTIME_BIT)
		destination->clientMessage.holdtime = source->clientMessage.holdtime;

	if (source->modifierBitField & SEQUENCE_MODIFIER_FXTIME_BIT)
		destination->clientMessage.fxtime = source->clientMessage.fxtime;

	if (source->modifierBitField & SEQUENCE_MODIFIER_TEXTCHANNEL_BIT)
		destination->textChannel = source->textChannel;

	if (source->modifierBitField & SEQUENCE_MODIFIER_SPEAKER_BIT)
	{
		if (destination->speakerName)
			Mem_Free(destination->speakerName);
		destination->speakerName = Mem_Strdup(source->speakerName);
	}

	if (source->modifierBitField & SEQUENCE_MODIFIER_LISTENER_BIT)
	{
		if (destination->listenerName)
			Mem_Free(destination->listenerName);
		destination->listenerName = Mem_Strdup(source->listenerName);
	}
}

void Sequence_BakeDefaults(sequenceCommandLine_s *destination, sequenceCommandLine_s *source)
{
	if (!destination)
		Sys_Error("Attempt to bake defaults into a non-existant command.");
	if (!source)
		Sys_Error("Attempt to bake defaults from a non-existant command.");

	destination->clientMessage.effect = source->clientMessage.effect;
	destination->clientMessage.r1 = source->clientMessage.r1;
	destination->clientMessage.g1 = source->clientMessage.g1;
	destination->clientMessage.b1 = source->clientMessage.b1;
	destination->clientMessage.a1 = source->clientMessage.a1;
	destination->clientMessage.r2 = source->clientMessage.r2;
	destination->clientMessage.g2 = source->clientMessage.g2;
	destination->clientMessage.b2 = source->clientMessage.b2;
	destination->clientMessage.a2 = source->clientMessage.a2;
	destination->clientMessage.x = source->clientMessage.x;
	destination->clientMessage.y = source->clientMessage.y;
	destination->clientMessage.fadein = source->clientMessage.fadein;
	destination->clientMessage.fadeout = source->clientMessage.fadeout;
	destination->clientMessage.holdtime = source->clientMessage.holdtime;
	destination->clientMessage.fxtime = source->clientMessage.fxtime;
	destination->textChannel = source->textChannel;

	if (destination->speakerName)
		Mem_Free(destination->speakerName);
	destination->speakerName = Mem_Strdup(source->speakerName);

	if (destination->listenerName)
		Mem_Free(destination->listenerName);
	destination->listenerName = Mem_Strdup(source->listenerName);
}

void Sequence_FlattenEntry(sequenceEntry_s *entry)
{
	sequenceCommandLine_s *post = NULL;
	for (sequenceCommandLine_s *pcmd = entry->firstCommand; pcmd != NULL; pcmd = pcmd->nextCommandLine)
	{
		switch (pcmd->commandType)
		{
		case SEQUENCE_COMMAND_SETDEFAULTS:
			Sequence_WriteDefaults(pcmd, &g_blockScopeDefaults);
			pcmd->commandType = SEQUENCE_COMMAND_NOOP;
			break;
		case SEQUENCE_COMMAND_MODIFIER:
			Sequence_WriteDefaults(pcmd, &g_blockScopeDefaults);
			break;
		case SEQUENCE_COMMAND_POSTMODIFIER:
			Sequence_WriteDefaults(pcmd, post);
			break;
		default:
			Sequence_BakeDefaults(pcmd, &g_blockScopeDefaults);
			post = pcmd;
			break;
		}
	}
}

void Sequence_FlattenAllEntries()
{
	for (sequenceEntry_s *pentry = g_sequenceList; pentry != NULL; pentry = pentry->nextEntry)
		Sequence_FlattenEntry(pentry);
}

void Sequence_ParseBuffer(byte *buffer, int bufferSize)
{
	char sym;

	g_lineNum = 1;
	g_scan = (char*)buffer;
	g_lineScan = (char*)buffer;
	Sequence_StripComments((char *)buffer, &bufferSize);
	Sequence_ResetDefaults(&g_fileScopeDefaults, 0);
	sym = Sequence_GetSymbol();
	while (sym)
	{
		switch (sym)
		{
		case '$':
			sym = Sequence_ParseModifierLine(NULL, 0);
			break;
		case '%':
			sym = Sequence_ParseEntry();
			break;
		case '!':
			sym = Sequence_ParseGlobalDataBlock();
			break;
		default:
			Sys_Error("Parsing error on line %d of %s.seq: expected '{' to start a\n new sentence block; found '%c' instead!",
				g_lineNum, g_sequenceParseFileName, sym);
		}
	}

	Sequence_ExpandAllGosubs();
	Sequence_FlattenAllEntries();
}

void Sequence_FreeCommand(sequenceCommandLine_s *kill)
{
	Z_Free((void*)kill->fireTargetNames);
	Z_Free((void*)kill->speakerName);
	Z_Free((void*)kill->listenerName);
	Z_Free((void*)kill->soundFileName);
	Z_Free((void*)kill->sentenceName);
	Z_Free((void*)kill->clientMessage.pName);
	Z_Free((void*)kill->clientMessage.pMessage);
}

void Sequence_FreeEntry(sequenceEntry_s *kill)
{
	Z_Free((void*)kill->entryName);
	Z_Free((void*)kill->fileName);

	for (sequenceCommandLine_s *pcmd = kill->firstCommand; pcmd != NULL; pcmd = pcmd->nextCommandLine)
	{
		kill->firstCommand = pcmd->nextCommandLine;
		Sequence_FreeCommand(pcmd);
	}

	Z_Free((void*)kill);
}

void Sequence_FreeSentence(sentenceEntry_s *sentenceEntry)
{
	Z_Free((void*)sentenceEntry->data);
	Z_Free((void*)sentenceEntry);
}

void Sequence_FreeSentenceGroupEntries(sentenceGroupEntry_s *groupEntry, qboolean purgeGlobals)
{
	sentenceEntry_s *save = NULL, *prev;

	for (sentenceEntry_s *pentry = groupEntry->firstSentence; pentry != NULL;)
	{
		if (!pentry->isGlobal || purgeGlobals)
		{
			if (save)
				save->nextEntry = pentry->nextEntry;
			else
				groupEntry->firstSentence = pentry->nextEntry;

			g_nonGlobalSentences--;
			prev = pentry;
			pentry = pentry->nextEntry;
			Sequence_FreeSentence(prev);
		}
		else
		{
			save = pentry;
			pentry = pentry->nextEntry;
		}
	}
}

void Sequence_FreeSentenceGroup(sentenceGroupEntry_s *groupEntry)
{
	Z_Free((void*)groupEntry->groupName);
	Z_Free((void*)groupEntry);
}

void Sequence_PurgeEntries(qboolean purgeGlobals)
{
	sequenceEntry_s *prev = NULL, *save = NULL;
	sentenceGroupEntry_s *sprev = NULL, *ssave = NULL;

	for (sequenceEntry_s *plist = g_sequenceList; plist != NULL; )
	{
		if (!plist->isGlobal || purgeGlobals)
		{
			if (save)
				save->nextEntry = plist->nextEntry;
			else
				g_sequenceList = plist->nextEntry;
			prev = plist;
			plist = plist->nextEntry;
			Sequence_FreeEntry(prev);
		}
		else
		{
			save = plist;
			plist = plist->nextEntry;
		}
	}

	for (sentenceGroupEntry_s *slist = g_sentenceGroupList; slist != NULL;)
	{
		Sequence_FreeSentenceGroupEntries(slist, purgeGlobals);
		if (slist->numSentences)
		{
			ssave = slist;
			slist = slist->nextEntry;
		}
		else
		{
			if (ssave)
				ssave->nextEntry = slist->nextEntry;
			else
				g_sentenceGroupList = slist->nextEntry;

			sprev = slist;
			slist = slist->nextEntry;
			Sequence_FreeSentenceGroup(sprev);
		}
	}
}

void Sequence_ParseFile(const char *fileName, qboolean isGlobal)
{
	byte *TempFile;
	char filePathName[256]; 
	int bufSize;

	Q_strncpy(g_sequenceParseFileName, fileName, 255);
	g_sequenceParseFileName[255] = 0;
	g_sequenceParseFileIsGlobal = isGlobal;
	_snprintf(filePathName, sizeof(filePathName), "sequences/%s.seq", fileName);
	TempFile = COM_LoadTempFile(filePathName, &bufSize);
	if (TempFile)
		Sequence_ParseBuffer(TempFile, bufSize);
	else
		Con_DPrintf(const_cast<char*>("WARNING: failed to locate sequence file %s\n"), fileName);
}

void Sequence_Init()
{
	Sequence_ParseFile("global", true);
}

sequenceEntry_s* SequenceGet(const char* fileName, const char* entryName)
{
	g_engdstAddrs.pfnSequenceGet(&fileName, &entryName);
	for (sequenceEntry_s *pseq = g_sequenceList; pseq != NULL; pseq = pseq->nextEntry)
	{
		if (!Q_strcasecmp(entryName, pseq->entryName))
			return pseq;
	}
	return NULL;
}

sentenceEntry_s* SequencePickSentence(const char *groupName, int pickMethod, int *picked)
{
	int num = 0;
	sentenceEntry_s *pentry = NULL;
	sentenceGroupEntry_s *pgroup = Sequence_FindSentenceGroup(groupName);

	g_engdstAddrs.pfnSequencePickSentence(&groupName, &pickMethod, &picked);

	if (pgroup)
	{
		num = RandomLong(0, pgroup->numSentences - 1);
		pentry = pgroup->firstSentence;
		for (int i = num; i != 0; i--)
			pentry = pentry->nextEntry;
	}

	if (picked != NULL)
		*picked = num;

	return pentry;
}

void Sequence_OnLevelLoad(const char* mapName)
{
	Sequence_PurgeEntries(false);
	Sequence_ParseFile(mapName, false);
}
