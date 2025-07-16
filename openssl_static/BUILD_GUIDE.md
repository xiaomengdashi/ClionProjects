# 编译指南

本文档详细说明了如何编译OpenSSL静态库项目的不同方法。

## 完整构建（推荐）

编译项目A和项目B：
```bash
./build.sh
```

## 单独编译项目B

### 前提条件
项目B依赖项目A的静态库，因此必须先确保项目A已编译。

### 方法1: 使用专用脚本（推荐）
```bash
./build_projectB_only.sh
```

这个脚本会：
- 检查项目A静态库是否存在
- 如果不存在，提供详细的错误信息和解决方案
- 只编译项目B

### 方法2: 手动CMake命令

#### 步骤1: 确保项目A已编译
如果项目A未编译，先编译项目A：
```bash
mkdir -p build
cd build
cmake ..
make ProjectA
cd ..
```

#### 步骤2: 单独编译项目B
```bash
cd build
make ProjectB
```

### 方法3: 使用CMake目标

在已配置的构建目录中：
```bash
cd build
cmake --build . --target ProjectB
```

## 编译验证

编译完成后，验证可执行文件：
```bash
# 检查文件是否存在
ls -la build/bin/ProjectB

# 运行程序
./build/bin/ProjectB
```

## 常见问题

### Q: 编译项目B时出现链接错误
A: 确保项目A的静态库存在：
```bash
ls -la build/lib/libProjectA.a
```
如果不存在，请先编译项目A。

### Q: 如何清理并重新编译项目B
A: 
```bash
# 清理项目B
cd build
make clean
# 或者删除项目B的目标文件
rm -f bin/ProjectB
rm -rf projectB/CMakeFiles/ProjectB.dir/

# 重新编译
make ProjectB
```

### Q: 如何在不同目录结构下编译
A: 如果项目结构发生变化，需要修改CMakeLists.txt中的路径：
```cmake
# 在projectB/CMakeLists.txt中
set(PROJECT_A_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../projectA)
```

## 依赖关系图

```
ProjectB (可执行文件)
    ↓ 依赖
ProjectA (静态库)
    ↓ 依赖  
OpenSSL (静态库)
```

## 编译输出

- **项目A静态库**: `build/lib/libProjectA.a`
- **项目B可执行文件**: `build/bin/ProjectB`

## 性能提示

- 使用 `-j` 参数并行编译：`make -j$(nproc) ProjectB`
- 只重新编译修改的文件：CMake会自动处理增量编译
- 使用 `ccache` 加速重复编译（如果已安装）