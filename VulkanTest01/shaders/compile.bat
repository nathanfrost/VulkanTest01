REM the -V flag instructs the compiler to create SPIR-V code
REM -H print human readable form of SPIR-V; turns on -V
REM -E print pre-processed GLSL; cannot be used with -l; errors will appear on stderr.
REM -S <stage> uses explicit stage specified, rather then the file extension.  valid choices are vert, tesc, tese, geom, frag, or comp
REM -D input is HLSL
REM -m memory leak mode
REM -q dump reflection query database
REM -s silent mode
REM -t multi-threaded mode
REM -v print version strings
REM -w suppress warnings (except as required by #extension : warn)
REM -x save 32-bit hexadecimal numbers as text, requires a binary option (e.g., -V)

%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shader.vert
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shader.frag
pause