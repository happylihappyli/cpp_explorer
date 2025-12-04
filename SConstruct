# SCons 构建脚本
import os

# 确保obj和bin目录存在
if not os.path.exists('obj'):
    os.makedirs('obj')
if not os.path.exists('bin'):
    os.makedirs('bin')

# 使用批处理文件包装器来解决路径中的空格问题
env = Environment(
    CXX='clang++.bat',
    CC='clang.bat',
    TOOLS=['mingw'],  # 使用MinGW工具链以避免MSVC参数格式
    CPPPATH=[],  # 包含目录
    LIBPATH=[]   # 库目录
)

print(f"使用的编译器: {env['CXX']}")

# 设置编译选项为C++17标准
env.Append(CXXFLAGS=['-std=c++17'])

# 添加Windows特定的库
env.Append(LIBS=['ole32', 'oleaut32', 'uuid', 'comctl32', 'shell32', 'user32', 'gdi32'])

# 添加警告标志
env.Append(CXXFLAGS=['-Wall'])

# 添加优化标志
env.Append(CXXFLAGS=['-O2'])

# 设置输出目录
env['OBJPREFIX'] = 'obj/'
env['PROGPREFIX'] = ''
env['PROGSUFFIX'] = '.exe'

fixed_sources = ['src/explorer.cpp']

fixed_program = env.Program(target='bin/myexplorer', source=fixed_sources)

# 设置默认目标
Default([fixed_program])