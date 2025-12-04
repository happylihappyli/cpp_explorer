@echo off
echo 正在清理编译生成的文件...

REM 使用SCons清理
scons -c

echo 清理完成!

pause