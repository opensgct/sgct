g++ -O3 -s -static main.cpp -o example.exe -I"%SGCT_ROOT_DIR%\include" -I"%OSGHOME%\include" -L"%SGCT_ROOT_DIR%\lib\mingw" -L"%OSGHOME%\lib" -lsgct32 -lopengl32 -lglu32 -lws2_32 -lOpenThreads.dll -losg.dll -losgutil.dll -losgdb.dll -losgga.dll -losgviewer.dll -static-libgcc -static-libstdc++
pause