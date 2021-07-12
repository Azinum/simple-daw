// config.c

config_parser_state Parser;

static u32 TypeSizes[MaxVariableType] = {
  0,
  sizeof(i32),
  sizeof(r32),
};

typedef enum config_token_type {
  TOK_IDENT,
  TOK_INT,
  TOK_FLOAT,
  TOK_NEWLINE,
  TOK_EOF,
} config_token_type;

typedef struct config_token {
  char* At;
  u32 Length;
  config_token_type Type;

  union {
    i32 Integer;
    float Float;
  };
} config_token;

static config_token CurrentToken = { .At = NULL, .Length = 0, .Type = TOK_EOF, };

static u8 IsNumber(char Ch);
static u8 IsAlpha(char Ch);
static variable_type TokenToVariableType(config_token_type TokenType);
static config_token ParseNumber(config_parser_state* P);
static config_token ParseIdent(config_parser_state* P);
static config_token NextToken(config_parser_state* P);
static void Next(config_parser_state* P);
static config_token NullTerminateToken(config_token Token);
static i32 Parse(config_parser_state* P);

static i32 PushVariable(config_parser_state* P, variable* Variable);
static u8 VariableExists(config_parser_state* P, const char* Name);
static variable* FindVariable(config_parser_state* P, const char* Name, u32 Length);

u8 IsNumber(char Ch) {
  return Ch >= '0' && Ch <= '9';
}

u8 IsAlpha(char Ch) {
  return (Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z');
}

variable_type TokenToVariableType(config_token_type TokenType) {
  switch (TokenType) {
    case TOK_INT:   return TypeInt32;
    case TOK_FLOAT: return TypeFloat32;
    default:        return TypeUndefined;
  }
}

config_token ParseNumber(config_parser_state* P) {
  u8 ShouldParseNumber = 1;
  u8 Dot = 0;
  CurrentToken.Type = TOK_INT;
  while (ShouldParseNumber) {
    u8 IsValid = 0;
    if (IsNumber(*P->Index) || *P->Index == 'x' || (*P->Index >= 'a' && *P->Index <= 'f') || (*P->Index >= 'A' && *P->Index <= 'F')) {
      IsValid = 1;
    }
    else if (*P->Index == '.') {
      CurrentToken.Type = TOK_FLOAT;
      Dot++;
      IsValid = 1;
    }
    if (IsValid) {
      P->Index++;
    }
    else {
      break;
    }
  }
  CurrentToken.Length = P->Index - CurrentToken.At;
  if (Dot > 1) {
    // Handle error
    CurrentToken.Type = TOK_EOF;
  }
  i32 ScanResult = 0;
  u32 NumBytesRead = 0;
  switch (CurrentToken.Type) {
    case TypeInt32:
      ScanResult = sscanf(CurrentToken.At, "%i", &CurrentToken.Integer);
      break;
    case TypeFloat32:
      ScanResult = sscanf(CurrentToken.At, "%f", &CurrentToken.Float);
      break;
  }
  (void)ScanResult;
  return CurrentToken;
}

config_token ParseIdent(config_parser_state* P) {
  while (IsAlpha(*P->Index) || *P->Index == '_') {
    P->Index++;
  }
  CurrentToken.Length = P->Index - CurrentToken.At;
  CurrentToken.Type = TOK_IDENT;
  return CurrentToken;
}

config_token NextToken(config_parser_state* P) {
  for (;;) {
    char Ch = *P->Index;
    Next(P);

    switch (Ch) {
      case EOF:
        CurrentToken.Type = TOK_EOF;
        return CurrentToken;
      case '\r':
      case '\n': {
        CurrentToken.Type = TOK_NEWLINE;
        return CurrentToken;
      }
      case ' ':
      case '\t':
      case '\v':
      case '\f':
        break;
      default: {
        if (IsNumber(Ch)) {
          return ParseNumber(P);
        }
        else if (IsAlpha(Ch) || Ch == '_') {
          return ParseIdent(P);
        }
        else {
          CurrentToken.Type = TOK_EOF;
          return CurrentToken;
        }
      }
    }
  }
  return CurrentToken;
}

config_token NullTerminateToken(config_token Token) {
  config_token Result = Token;

  Result.At[Result.Length] = '\0';

  return Result;
}

void Next(config_parser_state* P) {
  CurrentToken.At = P->Index++;
  CurrentToken.Length = 1;
}

i32 Parse(config_parser_state* P) {
  P->Index = &P->Source.Data[0];
  u8 ShouldParse = 1;
  config_token Token;
  while (ShouldParse) {
    Token = NextToken(P);
    switch (Token.Type) {
      case TOK_IDENT: {
        variable* Variable = FindVariable(P, Token.At, Token.Length);
        if (Variable) {
          for (i32 FieldIndex = 0; FieldIndex < Variable->NumFields; ++FieldIndex) {
            Token = NextToken(P);
            if (TokenToVariableType(Token.Type) == Variable->Type) {
              switch (Variable->Type) {
                case TypeInt32: {
                  *((i32*)Variable->Data + FieldIndex) = Token.Integer;
                  break;
                }
                case TypeFloat32: {
                  *((r32*)Variable->Data + FieldIndex) = Token.Float;
                  break;
                }
                default:
                  break;
              }
            }
            else {
              // Handle
            }
          }
        }
        break;
      }
      case TOK_EOF: {
        ShouldParse = 0;
        break;
      }
      default:
        break;
    }
  }

  return NoError;
}

i32 PushVariable(config_parser_state* P, variable* Variable) {
  ListPush(P->Variables, P->VariableCount, *Variable);
  return NoError;
}

// Slow linear search :/
// TODO(lucas): Implement hash table for variable locations
u8 VariableExists(config_parser_state* P, const char* Name) {
  for (i32 Index = 0; Index < P->VariableCount; ++Index) {
    variable* Variable = &P->Variables[Index];
    if (!strcmp(Variable->Name, Name)) {
      return 1;
    }
  }
  return 0;
}

variable* FindVariable(config_parser_state* P, const char* Name, u32 Length) {
  for (i32 Index = 0; Index < P->VariableCount; ++Index) {
    variable* Variable = &P->Variables[Index];
    u32 NameLength = strlen(Variable->Name);
    if (NameLength == Length) {
      if (!strncmp(Variable->Name, Name, Length)) {
        return Variable;
      }
    }
  }
  return NULL;
}

i32 ConfigParserInit() {
  i32 Result = NoError;

  config_parser_state* P = &Parser;
  P->Variables = NULL;
  P->VariableCount = 0;

  DefineVariable("sample_rate", &G_SampleRate, 1, TypeInt32);
  DefineVariable("frames_per_buffer", &G_FramesPerBuffer, 1, TypeInt32);

  DefineVariable("window_width", &G_WindowWidth, 1, TypeInt32);
  DefineVariable("window_height", &G_WindowHeight, 1, TypeInt32);
  DefineVariable("fullscreen", &G_FullScreen, 1, TypeInt32);
  DefineVariable("vsync", &G_Vsync, 1, TypeInt32);
  DefineVariable("audio_input", &G_AudioInput, 1, TypeInt32);

  DefineVariable("ui_color_background", &UIColorBackground, 3, TypeFloat32);
  DefineVariable("ui_color_accept", &UIColorAccept, 3, TypeFloat32);
  DefineVariable("ui_color_decline", &UIColorDecline, 3, TypeFloat32);
  DefineVariable("ui_color_standard", &UIColorStandard, 3, TypeFloat32);
  DefineVariable("ui_color_light", &UIColorLight, 3, TypeFloat32);
  DefineVariable("ui_color_inactive", &UIColorInactive, 3, TypeFloat32);
  DefineVariable("ui_color_not_present", &UIColorNotPresent, 3, TypeFloat32);

  DefineVariable("ui_button_size", &UIButtonSize, 2, TypeFloat32);
  return Result;
}

i32 ConfigRead() {
  i32 Result = NoError;
  config_parser_state* P = &Parser;

  char Path[MAX_PATH_SIZE];
  snprintf(Path, MAX_PATH_SIZE, "%s/%s", HomePath(), ".sdaw");

  if ((Result = ReadFileAndNullTerminate(Path, &P->Source)) != NoError) {
    Result = ConfigWrite(Path);  // Write default config
  }
  else {
    return Parse(P);
  }
  return Result;
}

i32 ConfigWrite(const char* Path) {
  i32 Result = NoError;

  FILE* File = fopen(Path, "w");
  if (File) {
    config_parser_state* P = &Parser;
    for (i32 Index = 0; Index < P->VariableCount; ++Index) {
      variable* Variable = &P->Variables[Index];
      fprintf(File, "%s", Variable->Name);
      for (i32 FieldIndex = 0; FieldIndex < Variable->NumFields; ++FieldIndex) {
        switch (Variable->Type) {
          case TypeInt32: {
            fprintf(File, " %i", *((i32*)Variable->Data + FieldIndex));
            break;
          }
          case TypeFloat32: {
            fprintf(File, " %lf", *((r32*)Variable->Data + FieldIndex));
            break;
          }
          default:
            break;
        }
      }
      fprintf(File, "\n");
    }
    fclose(File);
  }
  else {
    fprintf(stderr, "Failed to write configuration file '%s'\n", Path);
  }
  return Result;
}

i32 DefineVariable(const char* Name, void* Data, u32 NumFields, variable_type Type) {
  i32 Result = NoError;
  config_parser_state* P = &Parser;

  variable Variable = (variable) {
    .Name = Name,
    .Data = Data,
    .NumFields = NumFields,
    .Type = Type,
  };
  if (!VariableExists(P, Name)) {
    Result = PushVariable(P, &Variable);
  }
  else {
    fprintf(stderr, "Variable '%s' has already been defined\n", Name);
  }
  return Result;
}

void ConfigParserFree() {
  config_parser_state* P = &Parser;
  ListFree(P->Variables, P->VariableCount);
  BufferFree(&P->Source);
}
