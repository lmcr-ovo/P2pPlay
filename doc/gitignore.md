# .gitignore 写法说明

`.gitignore` 用来告诉 Git 哪些文件不需要纳入版本管理。一般应该忽略构建产物、IDE 配置、日志、临时文件和测试输出。

## 基本规则

忽略某个文件：

```gitignore
log.txt
```

忽略某类文件：

```gitignore
*.exe
*.obj
*.log
```

忽略目录：

```gitignore
cmake-build-*/
.idea/
```

只匹配项目根目录下的文件或目录：

```gitignore
/build/
/CMakeCache.txt
```

## 通配符

`*` 匹配任意字符：

```gitignore
cmake-build-*/
*.user
```

`?` 匹配单个字符：

```gitignore
test?.log
```

`[]` 匹配字符范围：

```gitignore
test[0-9].log
```

## 反忽略

使用 `!` 可以把前面忽略的文件重新加入：

```gitignore
*.log
!important.log
```

注意：如果父目录已经被忽略，单独反忽略子文件通常不生效，需要同时反忽略父目录。

## 本项目建议

Qt/CMake 项目通常应该忽略：

```gitignore
cmake-build-*/
CMakeFiles/
CMakeCache.txt
*_autogen/
*.exe
*.obj
*.dll
.idea/
```

当前项目的 UDP 测试会生成接收文件和日志，因此也忽略：

```gitignore
recv_*
log.txt
```

源码、文档、`CMakeLists.txt`、协议头文件和测试代码应该保留在 Git 中。

## 已经被 Git 跟踪的文件

`.gitignore` 只对“尚未被 Git 跟踪”的文件生效。如果某个文件已经提交过，后来再加入 `.gitignore`，Git 仍会继续跟踪它。

可以用下面命令停止跟踪，但保留本地文件：

```bash
git rm --cached path/to/file
```

停止跟踪目录：

```bash
git rm -r --cached cmake-build-debug
```

然后重新提交 `.gitignore` 即可。
