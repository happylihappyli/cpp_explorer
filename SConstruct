# SCons 构建脚本
import os
import subprocess
import datetime

# 记录开始时间
start_time = datetime.datetime.now()
print(f"编译开始时间: {start_time.strftime('%Y-%m-%d %H:%M:%S')}")

# 自动kill正在运行的myexplorer.exe进程
try:
    result = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq myexplorer.exe'], capture_output=True, text=True)
    if 'myexplorer.exe' in result.stdout:
        print("发现正在运行的myexplorer.exe进程，正在终止...")
        subprocess.run(['taskkill', '/F', '/IM', 'myexplorer.exe'], capture_output=True)
        print("myexplorer.exe进程已终止")
except Exception as e:
    print(f"终止进程时出错: {e}")

# 确保obj和bin目录存在
if not os.path.exists('obj'):
    os.makedirs('obj')
if not os.path.exists('bin'):
    os.makedirs('bin')

# 使用批处理文件包装器来解决路径中的空格问题
env = Environment(
    ENV = os.environ,
    CXX='clang++.bat',
    CC='clang.bat',
    TOOLS=['mingw'],  # 使用MinGW工具链以避免MSVC参数格式
    CPPPATH=[],  # 包含目录
    LIBPATH=[]   # 库目录
)

print(f"使用的编译器: {env['CXX']}")

# 设置编译选项为C++17标准
env.Append(CXXFLAGS=['-std=c++17'])

# 添加UTF-8编码支持
env.Append(CXXFLAGS=['-finput-charset=UTF-8', '-fexec-charset=UTF-8'])

# 添加UNICODE支持
env.Append(CPPDEFINES=['UNICODE', '_UNICODE'])

# 添加Windows特定的库
env.Append(LIBS=['ole32', 'oleaut32', 'uuid', 'comctl32', 'shell32', 'user32', 'gdi32', 'shlwapi', 'comdlg32'])

# 添加警告标志
env.Append(CXXFLAGS=['-Wall'])

# 添加优化标志
env.Append(CXXFLAGS=['-O2'])

# 添加Windows子系统标志，防止默认创建控制台窗口
env.Append(LINKFLAGS=['-mwindows'])

# 设置输出目录
env['OBJPREFIX'] = 'obj/'
env['PROGPREFIX'] = ''
env['PROGSUFFIX'] = '.exe'

# 配置资源编译器 (使用 llvm-rc)
env['RC'] = 'llvm-rc'
env['RCCOM'] = '$RC $RCFLAGS /FO $TARGET $SOURCE'

# 显式编译资源文件
res_obj = env.Command('src/obj/resource.res', 'src/resource.rc', '$RCCOM')

fixed_sources = ['src/explorer.cpp', 'src/favorites.cpp', 'src/tree_utils.cpp', 'src/log.cpp', 'src/file_utils.cpp', 'src/notification_handlers.cpp', 'src/go_button_handler.cpp', 'src/settings.cpp']
# 将资源对象文件添加到源列表
fixed_sources.extend(res_obj)

fixed_program = env.Program(target='bin/myexplorer', source=fixed_sources)

# 设置默认目标
Default([fixed_program])

# 记录结束时间
end_time = datetime.datetime.now()
duration = end_time - start_time
print(f"编译结束时间: {end_time.strftime('%Y-%m-%d %H:%M:%S')}")
print(f"编译耗时: {duration.total_seconds():.2f} 秒")