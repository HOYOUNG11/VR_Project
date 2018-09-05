#include "Editor.h"
#include <crtdbg.h>
int main(int argc, char* argv[]) {
#ifdef _DEBUG
	// Put memory allocation number if you want to debug mem leak
	//_CrtSetBreakAlloc();
#endif
	Editor* editor = new Editor(1000, 720);
	if (editor->Initialize()) {
		editor->Run();
	}
	delete editor;
#ifdef _DEBUG
	// Perform complete C library termination procedures to pass memory leak test;
	_cexit();
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}
