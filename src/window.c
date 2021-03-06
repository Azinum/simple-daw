// window.c

window Window;

u8 KeyDown[GLFW_KEY_LAST] = {0};
u8 KeyPressed[GLFW_KEY_LAST] = {0};
u8 GamepadButtonDown[MAX_GAMEPAD_BUTTON] = {0};
u8 GamepadButtonPressed[MAX_GAMEPAD_BUTTON] = {0};

float JoystickValues[MAX_JOYSTICK] = {0};
u32 JoystickCount = 0;
u8 JoystickPresent = 0;

static const u8* GamepadButtonStates = NULL;
static i32 GamepadButtonCount = 0;
static const float* GamepadAxes = NULL;
static i32 GamepadAxisCount = 0;

static void ErrorCallback(i32 ErrCode, const char* ErrString);

void ErrorCallback(i32 ErrCode, const char* ErrString) {
  (void)ErrCode;
  (void)ErrString;
  // fprintf(stderr, "GLFW error(%i): %s\n", ErrCode, ErrString);
}

i32 WindowWidth() {
  return Window.Width;
}

i32 WindowHeight() {
  return Window.Height;
}

// Returns a reference to the location of the stored callback. This is so that the user can NULL-ify the callback, thus unsubscribing from the callback invocations.
window_resize_callback* WindowAddResizeCallback(window_resize_callback Callback) {
  if (Window.WindowResizeCallbackCount < MAX_RESIZE_CALLBACK) {
    window_resize_callback* CallbackReference = &Window.WindowResize[Window.WindowResizeCallbackCount++];
    *CallbackReference = Callback;
    return CallbackReference;
  }
  return NULL;
}

static void FrameBufferSizeCallback(GLFWwindow* Win, i32 Width, i32 Height) {
#if __APPLE__
  glfwGetWindowSize(Win, &Width, &Height);
#else
  // glViewport(0, 0, Width, Height);
  glViewport(0, 0, Width, Height);
#endif
  Window.Width = Width;
  Window.Height = Height;
  for (u32 CallbackIndex = 0; CallbackIndex < Window.WindowResizeCallbackCount; ++CallbackIndex) {
    window_resize_callback Callback = Window.WindowResize[CallbackIndex];
    if (Callback) {
      Callback(Window.Width, Window.Height);
    }
  }
}

static void ConfigureOpenGL() {
  glEnable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glShadeModel(GL_FLAT);
  glEnable(GL_TEXTURE_2D);
  glAlphaFunc(GL_GREATER, 1);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static i32 WindowOpen(u32 Width, u32 Height, const char* Title, u8 Vsync, u8 FullScreen) {
  Window.Title = Title;
  Window.FullScreen = FullScreen;
  Window.Width = Window.InitWidth = Width;
  Window.Height = Window.InitHeight = Height;
  Window.WindowResizeCallbackCount = 0;

  glfwSetErrorCallback(ErrorCallback);

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_FOCUSED, GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

#if defined(__APPLE__)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  if (Window.FullScreen) {
    const GLFWvidmode* Mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    Window.Width = Mode->width;
    Window.Height = Mode->height;
    Window.Window = glfwCreateWindow(Window.Width, Window.Height, Title, glfwGetPrimaryMonitor(), NULL);
  }
  else {
    Window.Window = glfwCreateWindow(Window.Width, Window.Height, Title, NULL, NULL);
  }
  if (!Window.Window) {
    fprintf(stderr, "Failed to create window\n");
    return -1;
  }

  glfwMakeContextCurrent(Window.Window);
  glfwSetFramebufferSizeCallback(Window.Window, FrameBufferSizeCallback);

  i32 Error = glewInit();
  if (Error != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(Error));
    return -1;
  }
  glfwSwapInterval(Vsync);
  ConfigureOpenGL();
  FrameBufferSizeCallback(Window.Window, Window.Width, Window.Height);
  return 0;
}

static void WindowToggleFullScreen() {
  Window.FullScreen = !Window.FullScreen;
  if (Window.FullScreen) {
    const GLFWvidmode* Mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    Window.Width = Mode->width;
    Window.Height = Mode->height;
  }
  else {
    Window.Width = Window.InitWidth;
    Window.Height = Window.InitHeight;
  }
  glfwSetWindowSize(Window.Window, Window.Width, Window.Height);
}

i32 WindowPollEvents() {
  glfwPollEvents();
  glfwGetCursorPos(Window.Window, &MouseX, &MouseY);

  for (u16 Index = 0; Index < GLFW_KEY_LAST; Index++) {
    i32 KeyState = glfwGetKey(Window.Window, Index);
    if (KeyState == GLFW_PRESS) {
      KeyPressed[Index] = !KeyDown[Index];
      KeyDown[Index] = 1;
    }
    else {
      KeyDown[Index] = 0;
      KeyPressed[Index] = 0;
    }
  }

  i32 LeftMouseButtonState = glfwGetMouseButton(Window.Window, 0);
  i32 RightMouseButtonState = glfwGetMouseButton(Window.Window, 1);
  i32 MiddleMouseButtonState = glfwGetMouseButton(Window.Window, 2);

  (LeftMouseButtonState && !(MouseState & (1 << 7))) ? MouseState |= (1 << 6) : (MouseState &= ~(1 << 6));
  LeftMouseButtonState ? MouseState |= (1 << 7) : (MouseState &= ~(1 << 7));
  (RightMouseButtonState && !(MouseState & (1 << 5))) ? MouseState |= (1 << 4) : (MouseState &= ~(1 << 4));
  RightMouseButtonState ? MouseState |= (1 << 5) : (MouseState &= ~(1 << 5));

  (MiddleMouseButtonState && !(MouseState & (1 << 3))) ? MouseState |= (1 << 2) : (MouseState &= ~(1 << 2));
  MiddleMouseButtonState ? MouseState |= (1 << 3) : (MouseState &= ~(1 << 3));

  if (KeyPressed[GLFW_KEY_F11]) {
    WindowToggleFullScreen();
  }

  if (KeyPressed[GLFW_KEY_ESCAPE] || glfwWindowShouldClose(Window.Window)) {
    return -1;
  }

  if ((JoystickPresent = glfwJoystickPresent(GLFW_JOYSTICK_1))) {
    GamepadButtonStates = glfwGetJoystickButtons(0, &GamepadButtonCount);
    GamepadAxes = glfwGetJoystickAxes(0, &GamepadAxisCount);
    for (u8 ButtonIndex = 0; ButtonIndex < MAX_GAMEPAD_BUTTON && ButtonIndex < GamepadButtonCount; ++ButtonIndex) {
      u8 KeyState = GamepadButtonStates[ButtonIndex];
      if (KeyState == GLFW_PRESS) {
        GamepadButtonPressed[ButtonIndex] = !GamepadButtonDown[ButtonIndex];
        GamepadButtonDown[ButtonIndex] = 1;
      }
      else {
        GamepadButtonDown[ButtonIndex] = 0;
        GamepadButtonPressed[ButtonIndex] = 0;
      }
    }
    if (GamepadAxes) {
      JoystickCount = GamepadAxisCount;
      for (i32 AxisIndex = 0; AxisIndex < MAX_JOYSTICK && AxisIndex < GamepadAxisCount; ++AxisIndex) {
        JoystickValues[AxisIndex] = GamepadAxes[AxisIndex];
      }
    }
    else {
      JoystickCount = 0;
    }
  }
  return 0;
}

void WindowSetTitle(const char* Title) {
  glfwSetWindowTitle(Window.Window, Title);
}

void WindowSwapBuffers() {
  glfwSwapBuffers(Window.Window);
}

void WindowClear(v3 Color) {
  glClearColor(Color.R, Color.G, Color.B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void WindowClose() {
  glfwDestroyWindow(Window.Window);
  glfwTerminate();
}
