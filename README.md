# TXTFST

A txt index for a task in CNSS Recruit 2024.

## Documentation

### txtfst-build

```shell

```

### txtfst-search

```shell

```

## Task

### 📚 图书管理系统

> 经典的 C 语言大作业？

香肠搞到了一批电子书，以下面的形式组织：

- 所有文件都在一个目录或其子目录下，文件均为 `.txt` 拓展名
- 文件第一行是标题（不包含第一行换行符），除去第一行都是正文（不包含第一行换行符）
- 纯文本形式，UTF-8 编码，LF 换行符

现在请你实现两个程序

- 索引器：建立一个目录下的所有文章的索引
    - 如果文章包含非法的 UTF-8 序列，应该报错
- 查询器：查询指定标题或正文的文章
    - 标题查询：若提供的字符串与文章标题完全相同时，则返回该文章的路径
    - 正文查询：若提供的字符串是文章正文中出现过的一个词，则返回该文章的路径
    - 如果查询包含非法的 UTF-8 序列，应该报错

#### 说明

- 正文查询时需要按一定规则对文章进行分词，为了统一起见，规则定义如下：
    - 文本中若干个连续的 `[A-Za-z0-9]` 定义为一个词
    - 忽略所有 `[A-Za-z0-9]` 以外的文本
    - 所有的词语全部转为小写
    - 例如：`Welcome to CNSS😎, Yaossg!` 应该分词为 `welcome` `to` `cnss` `yaossg`
- 建立的索引应该能够持久化到硬盘上，索引器和查询器应该是两个独立的程序
    - 类似于这样：
  ```shell
  $ ./build-index /path/to/library
  Building index...
  OK, index is located at /path/to/library.idx
  $ ./search-index /path/to/library.idx --title "Sausage"
  /path/to/library/246.txt
  $ ./search-index /path/to/library.idx --content "sausage"
  /path/to/library/123.txt
  /path/to/library/456.txt
  /path/to/library/789.txt
  ```
- 如果还不清楚某些细节可以咨询 @Yaossg

#### 😎 得分标准

- 标题查询 30%
- 正文查询 70%
  注意：建立索引可以多花一些时间，尽量让查询快一些。我会进行一些基本的性能测试。

#### ✍ 提交要求

完成题目后将 wp 用 Markdown 写完后导出为 pdf，统一命名为`[CNSS Recruit] 用户名 - 题目名称` 发送至 `yaossg@qq.com`：

- 源代码
- 测试代码、运行截图
- 使用文档

此外，请在 wp 中回答下面的问题：

- 你使用了什么数据结构来建立索引？
- 索引的空间复杂度是多少？
- 查询的时间复杂度是多少？
- UTF-8 的支持有哪些需要注意的地方？