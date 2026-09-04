call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "C:\Users\Josue B\Desktop\Josue B\Documents\Jonatan Baron\Proyectos\proyecto de IA artistica\Motor Grafico"
cl /nologo /EHsc /std:c++17 /utf-8 /O2 /MD /I "." /I "external\asmjit" /I "..\..\Lenguaje Hermetico\contrato" test_dump_scene.cpp render\scene.cpp os\win32\mem.cpp os\win32\file.cpp /Fe"build\test_dump_scene.exe"
