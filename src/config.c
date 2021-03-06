// config.c

config_parser_state Parser;

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
    f32 Float;
    f32 Number;
  };
} config_token;

static config_token CurrentToken = { .At = NULL, .Length = 0, .Type = TOK_EOF, };

static u8 IsNumber(char Ch);
static u8 IsAlpha(char Ch);
static config_token ParseNumber(config_parser_state* P);
static config_token ParseIdent(config_parser_state* P);
static config_token NextToken(config_parser_state* P);
static void Next(config_parser_state* P);
static i32 Parse(config_parser_state* P);

static i32 InsertVariable(config_parser_state* P, variable* Variable);
static u8 VariableExists(config_parser_state* P, const char* Name);
static variable* FindVariable(config_parser_state* P, const char* Name, u32 Length);

u8 IsNumber(char Ch) {
  return Ch >= '0' && Ch <= '9';
}

u8 IsAlpha(char Ch) {
  return (Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z');
}

config_token ParseNumber(config_parser_state* P) {
  u8 ShouldParseNumber = 1;
  u8 Dot = 0;
  CurrentToken.Type = TOK_INT;
  while (ShouldParseNumber) {
    u8 IsValid = 0;
    if (IsNumber(*P->Index) || *P->Index == '-' || *P->Index == 'x' || (*P->Index >= 'a' && *P->Index <= 'f') || (*P->Index >= 'A' && *P->Index <= 'F')) {
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
  switch (CurrentToken.Type) {
    case TypeInt32: {
      f32 Value = 0;
      ScanResult = sscanf(CurrentToken.At, "%f", &Value);
      CurrentToken.Number = Value;
      break;
    }
    case TypeFloat32: {
      f32 Value = 0;
      ScanResult = sscanf(CurrentToken.At, "%f", &Value);
      CurrentToken.Number = Value;
      break;
    }
    default:
      break;
  }
  (void)ScanResult;
  return CurrentToken;
}

config_token ParseIdent(config_parser_state* P) {
  while (IsAlpha(*P->Index) || *P->Index == '/' || *P->Index == '.' || *P->Index == '-' || *P->Index == '_') {
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
      case EOF: {
        CurrentToken.Type = TOK_EOF;
        return CurrentToken;
      }
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
        if (IsNumber(Ch) || Ch == '-') {
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
          for (u32 FieldIndex = 0; FieldIndex < Variable->NumFields; ++FieldIndex) {
            Token = NextToken(P);
            switch (Variable->Type) {
              case TypeInt32: {
                *((i32*)Variable->Data + FieldIndex) = (i32)Token.Number;
                break;
              }
              case TypeFloat32: {
                *((f32*)Variable->Data + FieldIndex) = (f32)Token.Number;
                break;
              }
              case TypeString: {
                char* String = (char*)(char**)(Variable->Data + FieldIndex);
                strncpy(String, Token.At, Token.Length);
                String[Token.Length] = '\0';
                break;
              }
              default:
                break;
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

i32 InsertVariable(config_parser_state* P, variable* Variable) {
  i32 Location = P->VariableCount;
  ht_key Key = HashString((char*)Variable->Name, strlen(Variable->Name));
  ListPush(P->Variables, P->VariableCount, *Variable);
  HtInsertElement(&P->VariableLocations, Key, Location);
  return NoError;
}

u8 VariableExists(config_parser_state* P, const char* Name) {
  return FindVariable(P, Name, strlen(Name)) != NULL;
}

variable* FindVariable(config_parser_state* P, const char* Name, u32 Length) {
  ht_key Key = HashString((char*)Name, Length);
  const ht_value* Value = HtLookup(&P->VariableLocations, Key);
  if (Value) {
    return &P->Variables[*Value];
  }
  return NULL;
}

i32 ConfigParserInit() {
  i32 Result = NoError;

  config_parser_state* P = &Parser;
  P->Variables = NULL;
  P->VariableCount = 0;
  P->VariableLocations = HtCreateEmpty();

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
  DefineVariable("ui_color_container", &UIColorContainer, 3, TypeFloat32);
  DefineVariable("ui_color_container_bright", &UIColorContainerBright, 3, TypeFloat32);
  DefineVariable("ui_color_button", &UIColorButton, 3, TypeFloat32);
  DefineVariable("ui_color_border", &UIColorBorder, 3, TypeFloat32);
  DefineVariable("ui_border_thickness", &UIBorderThickness, 1, TypeFloat32);

  DefineVariable("ui_button_size", &UIButtonSize, 2, TypeFloat32);
  DefineVariable("ui_margin", &UIMargin, 1, TypeFloat32);
  DefineVariable("ui_text_size", &UITextSize, 1, TypeInt32);
  DefineVariable("ui_text_kerning", &UITextKerning, 1, TypeFloat32);
  DefineVariable("ui_text_leading", &UITextLeading, 1, TypeFloat32);
  DefineVariable("ui_font_path", &UIFontPath, 1, TypeString);

  DefineVariable("gamepad_button_up", &G_GamepadButtonUp, 1, TypeInt32);
  DefineVariable("gamepad_button_down", &G_GamepadButtonDown, 1, TypeInt32);
  DefineVariable("gamepad_button_left", &G_GamepadButtonLeft, 1, TypeInt32);
  DefineVariable("gamepad_button_right", &G_GamepadButtonRight, 1, TypeInt32);

  DefineVariable("pa_input_device", &G_PaInputDevice, 1, TypeInt32);
  DefineVariable("pa_output_device", &G_PaOutputDevice, 1, TypeInt32);

  DefineVariable("stream_buffer_size_multiple", &G_StreamBufferSizeMultiple, 1, TypeInt32);
  DefineVariable("stream_buffer_denom", &G_StreamBufferDenom, 1, TypeInt32);

  return Result;
}

i32 ConfigRead() {
  i32 Result = NoError;
  config_parser_state* P = &Parser;

  char Path[MAX_PATH_SIZE];
  snprintf(Path, MAX_PATH_SIZE, "%s/.config/sdaw/%s", HomePath(), CONFIG_PATH);

  // NOTE(lucas): We free the old source buffer, in cases where
  // we want to re-read the configuration file, so that we do not get a memory leak
  if (P->Source.Data) {
    BufferFree(&P->Source);
  }

  if ((Result = ReadFileAndNullTerminate(Path, &P->Source)) != NoError) {
    if ((Result = ConfigWrite(Path)) != NoError) {
      snprintf(Path, MAX_PATH_SIZE, "%s/%s", HomePath(), CONFIG_PATH);
      if ((Result = ReadFileAndNullTerminate(Path, &P->Source)) != NoError) {
        Result = ConfigWrite(Path);  // Write default config
      }
    }
    else {
      Result = ConfigWrite(Path);  // Write default config
    }
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
    for (u32 Index = 0; Index < P->VariableCount; ++Index) {
      variable* Variable = &P->Variables[Index];
      fprintf(File, "%s", Variable->Name);
      for (u32 FieldIndex = 0; FieldIndex < Variable->NumFields; ++FieldIndex) {
        switch (Variable->Type) {
          case TypeInt32: {
            fprintf(File, " %i", *((i32*)Variable->Data + FieldIndex));
            break;
          }
          case TypeFloat32: {
            fprintf(File, " %g", *((f32*)Variable->Data + FieldIndex));
            break;
          }
          case TypeString: {
            fprintf(File, " %s", (char*)(char**)(Variable->Data + FieldIndex));
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
    Result = InsertVariable(P, &Variable);
  }
  else {
    fprintf(stderr, "Variable '%s' has already been defined\n", Name);
  }
  return Result;
}

void ConfigParserFree() {
  config_parser_state* P = &Parser;
  ListFree(P->Variables, P->VariableCount);
  HtFree(&P->VariableLocations);
  BufferFree(&P->Source);
}
