# 外部排序算法实现文档

> **项目仓库**：[external-sort](https://github.com/SiriLee/external-sort.git)
>
> **编译环境**：Linux (WSL2) / g++ (C++17)
>
> **编程综合实践 · 个人项目**

---

## 一、问题描述

外部排序（External Sorting）是处理海量数据排序的核心算法。当待排序数据规模远超内存容量时，无法将所有数据一次性载入内存进行排序，必须借助外部存储（磁盘）进行分阶段处理。

本次实验要求实现一个完整的**二阶段外部排序系统**：

1. **第一阶段（生成初始归并段）**：从输入流读取整数序列，在内存限制为最多同时容纳 **k** 个元素的条件下，使用置换-选择排序算法生成若干个有序的初始归并段（run），写入磁盘文件。
2. **第二阶段（k 路归并）**：基于最佳归并树策略，将第一阶段生成的所有归并段通过多趟 k 路平衡归并，最终合并为一个全局有序的输出文件。

其中 **k** 作为统一的控制参数，同时限制第一阶段的内存缓冲区大小和第二阶段的归并路数上限。

---

## 二、算法设计与原理

### 2.1 第一阶段：置换-选择排序（Replacement-Selection Sort）

#### 2.1.1 算法思想

传统的分块内排序方法（如将文件切成若干大小为 k 的块分别排序）生成的归并段数量约为 $N/k$。而置换-选择排序利用"雪崩效应"（snowplow effect），在典型随机数据下能生成平均长度为 $2k$ 的归并段，有效减少归并段数量，降低后续归并开销。

核心思想：维护一个容量为 k 的最小堆，每次输出堆顶最小值，同时从输入读取下一个数。若新数 $\ge$ 刚刚输出的数，则它仍属于当前归并段；否则将其"冻结"（标记为属于下一归并段）。当堆中所有元素都属于下一段时，当前段结束，开始新的一段。

#### 2.1.2 算法流程

```
输入: 输入文件流 input, 内存容量 k
输出: 归并段文件列表 run_files

1. 从 input 预读 k 个数，构建最小堆 pq，每个元素标记为归并段 0
2. current_run_index = 0
3. while pq 非空:
   a. 弹出堆顶 (index, value)
   b. 若 index > current_run_index:
      - 关闭当前归并段文件
      - current_run_index = index
   c. 将 value 写入当前归并段文件
   d. 从 input 读取下一个数 next_num
   e. 若读取成功:
      - 若 next_num >= value: new_index = index（同段）
      - 否则: new_index = index + 1（下一段）
      - 将 (new_index, next_num) 入堆
4. 关闭最后一个归并段文件
5. 返回所有归并段文件路径
```

#### 2.1.3 数据结构

- **最小堆**：`std::priority_queue<Entry, vector<Entry>, greater<Entry>>`，其中 `Entry = pair<size_t, int>`，`first` 为归并段索引（越小越优先），`second` 为数据值。`greater<Entry>` 确保按 (段索引, 数值) 的字典序升序排列。

- **段索引语义**：段索引既标识元素所属归并段，又作为堆排序的主键——较小段索引的元素优先出堆，段索引相同时较小数值优先。这保证了每段内数据的严格有序性。

#### 2.1.4 关键性质

- 当堆中所有元素的段索引均 > current_run_index 时，当前段自然结束，下一个段索引的元素开始输出。这实现了段的自动切换，无需显式计数。
- 每个归并段文件的整数严格升序排列，且所有归并段均非空（除非输入本身为空）。
- 归并段数据总量与原文件完全一致。

---

### 2.2 第二阶段：最佳归并树 + k 路归并

#### 2.2.1 最佳归并树策略

第一阶段生成的各归并段长度可能差异很大。若简单按顺序合并，长段可能被反复参与多趟归并，造成大量无效 I/O。最佳归并树（Optimal Merge Tree，即多路 Huffman 树）策略每次选取当前长度最小的 k 个归并段进行合并，使得长度较大的段参与归并的次数最少，从而最小化总 I/O 量。

#### 2.2.2 虚拟段补齐

对于 k 路归并树，若初始段数 n 不满足 $(k-1) \mid (n-1)$（即最后一次归并不能凑齐 k 路），需要补齐虚拟段。补齐数量为：

$$d = (k - 1 - (n - 1) \bmod (k - 1)) \bmod (k - 1)$$

每个虚拟段长度为 0，参与归并但不贡献数据，确保 Huffman 树的归并过程正确收敛到单一根节点。

#### 2.2.3 k 路归并合并

每次合并使用一个最小堆维护 k 个输入流当前的最小值：

```
_Merge(to_merge, run_files):
1. 打开 to_merge 中 k 个归并段的输入流
2. 读每个流的第一个数，建最小堆 pq: (value, file_index)
3. 创建输出文件 merged_file
4. while pq 非空:
   a. 弹出 (val, idx)，写入输出文件
   b. 从输入流 idx 读取下一个数，若存在则入堆
5. 关闭所有流，返回 merged_file
```

#### 2.2.4 完整归并流程

```
KWayMerge(run_files, k):
1. 若 run_files 为空或 k < 2，返回空字符串
2. 计算并补齐虚拟段
3. 建立最小堆 pq: (data_count, file_index)
4. while pq.size() > 1:
   a. 取长度最小的 k 个段
   b. 调用 _Merge 合并为一个新段
   c. 新段加入 pq
5. 将最终段重命名为 sorted_output.txt
6. 清理临时文件，返回输出路径
```

---

### 2.3 算法正确性证明（简述）

**第一阶段正确性**：最小堆始终保证当前可输出的最小元素位于堆顶。段索引机制确保属于当前段的元素优先于后续段元素输出。由于堆大小恒为 k，内存使用始终在限制内。

**第二阶段正确性**：每次 k 路归并从 k 个有序序列的首元素中取最小者输出，这是经典 k 路归并的正确性保证。最佳归并树策略仅影响合并顺序，不改变归并结果——无论以何种顺序归并，有序序列的归并结果总是全局有序的。虚拟段补齐保证了 Huffman 树的严格 k 叉结构。

---

## 三、实现细节

### 3.1 文件组织

| 路径 | 作用 |
|------|------|
| `save/` | 所有临时文件目录 |
| `save/run_<i>.txt` | 初始归并段及中间合并结果文件 |
| `save/sorted_output.txt` | 最终排序输出 |
| `benchmark_big.input` | 测试程序生成的输入数据 |

### 3.2 关键实现技术

#### 3.2.1 数据计数缓存（`_data_count_cache`）

在 `GenerateInitialRuns` 阶段，每个归并段关闭时将其中数据条数记录到 `_data_count_cache`。在 `_Merge` 阶段，新合并文件的数据条数同样被缓存。这使得 `KWayMerge` 中构建 Huffman 树时能快速获取每个段的长度，无需重新扫描文件。

使用 `std::unordered_map<std::string, std::size_t>` 实现，以文件名为键。在只读访问时使用 `at()` 而非 `operator[]`，避免键不存在时静默插入默认值。

#### 3.2.2 错误处理

- **目录检查**：`GenerateInitialRuns` 开头检查 `save/` 目录是否存在，不存在则创建。
- **文件打开检查**：所有文件打开操作后均检查 `is_open()`，失败时安全返回。
- **写操作检查**：`_Merge` 中检查 `IntWriter` 返回值，写入失败时关闭流并提前返回。

#### 3.2.3 临时文件清理

`KWayMerge` 返回前遍历所有中间文件（初始归并段、虚拟段、中间合并结果）并调用 `std::filesystem::remove` 删除，确保不残留临时文件。

#### 3.2.4 防御性编程

- `k < 2` 时（包括 `k = 1`）直接返回空字符串，避免除零错误。
- 使用 `std::size_t` 作为段索引类型，语义明确且范围足够。
- 使用 `std::filesystem::rename` 将最终合并文件原子性地移动为目标输出文件名。

---

## 四、复杂度分析

### 4.1 时间复杂度

| 阶段 | 操作 | 复杂度 |
|------|------|--------|
| 第一阶段 | 每个元素一次堆操作 | $O(N \log k)$ |
| 第二阶段-单趟归并 | k 个流每输出一个元素一次堆操作 | $O(N \log k)$ |
| 第二阶段-归并趟数 | Huffman 树高度 | $O(\log_k R)$，R 为初始段数 |

**总体时间复杂度**：$O(N \log k + N \log k \cdot \log_k R) = O(N \log k \cdot \log_k R)$

在典型随机数据下，置换-选择排序产生的初始段数 $R \approx N/(2k)$，因此：

$$\log_k R \approx \log_k(N/2k) = \log_k N - \log_k(2k)$$

当 $k=64$ 且 $N=10^7$ 时，$R \approx 78125$，$\log_{64} 78125 \approx 2.7$，归并约需 3 趟。

### 4.2 空间复杂度

- **第一阶段内存**：最小堆最多容纳 k 个 `pair<size_t, int>` 元素，空间为 $O(k)$。
- **第二阶段内存**：归并时每个输入流一个缓冲区，空间为 $O(k)$。
- **临时文件缓存**：`_data_count_cache` 存储 $O(R)$ 条记录。

**总体空间复杂度**：$O(k + R)$，其中 $R$ 为归并段数量。由于 $R \ll N$，内存开销远小于数据总量。

### 4.3 I/O 次数

- **读操作**：第一阶段每个元素读一次（共 N 次），每趟归并每个元素读一次（共约 $\lceil\log_k R\rceil \cdot N$ 次）。
- **写操作**：第一阶段每个元素写一次（共 N 次），每趟归并每个元素写一次（共约 $\lceil\log_k R\rceil \cdot N$ 次）。

总计约 $(1 + \lceil\log_k R\rceil)$ 次全量数据扫描。

---

## 五、实验结果

### 5.1 测试环境

| 项目 | 配置 |
|------|------|
| 操作系统 | Linux (WSL2 on Windows 11), kernel 6.6.87 |
| 编译器 | g++ (C++17, -O2) |
| CPU | — |
| 数据规模 | N = 10,000,000（Linux）/ N = 10,000（Windows） |
| 数据范围 | [0, 2³⁰) 均匀随机分布 |
| 随机种子 | 20260706（固定） |

### 5.2 测试结果

#### Linux (WSL2)

| k | 第一阶段 | 第二阶段 | 状态 |
|---|----------|----------|------|
| 4 | 正确通过（所有归并段有序且非空，总数一致） | 正确通过（输出与标准答案逐位一致） | ✓ |
| 64 | 正确通过 | 正确通过 | ✓ |

#### Windows

| k | 第一阶段 | 第二阶段 | 状态 |
|---|----------|----------|------|
| 4 | 正确通过 | 正确通过 | ✓ |
| 64 | 正确通过 | 正确通过 | ✓ |

> **注**：Windows 环境下 N = 10,000,000 因系统内存限制无法运行，将 `main.cpp` 中 `N` 改为 `10000` 后测试通过。

### 5.3 正确性验证要点

- **第一阶段验证**（`VerifyInitialRuns`）：
  - 检查每个归并段文件非空
  - 检查每个归并段内元素严格升序
  - 检查所有归并段的元素总数与原文件一致

- **第二阶段验证**（`VerifyFinalSorted` + `CheckFinalOutput`）：
  - 检查输出文件全局升序
  - 检查输出文件的元素集合与原文件完全一致（重新生成标准答案并逐元素比对）

---

## 六、开发过程与版本管理

本项目全程使用 Git 进行版本管理，共 22 次提交。开发遵循渐进式迭代策略：

### 6.1 提交历史概览

| 阶段 | 关键提交 | 内容 |
|------|----------|------|
| 初始化 | `core:init project` | 搭建项目框架 |
| 第一阶段实现 | `implement function GenerateInitialRuns` | 完成置换-选择排序 |
| | `use save/ as temp directory` | 统一临时文件目录 |
| 第二阶段实现 | `add _data_count_cache` | 引入数据计数缓存机制 |
| | `implement private function _Merge` | 实现 k 路归并辅助函数 |
| | `init Huffman pq` | 构建最佳归并树优先队列 |
| | `Finish the external_sort` | 完成 KWayMerge 主逻辑 |
| 修复与优化 | `check before close output stream` | 防御性检查 |
| | `Use at() instead of op[]` | 修复 map 静默插入问题 |
| | `Remove useless idx <=0` | 清理无效代码 |
| | `Remove temp files before return` | 临时文件清理 |

### 6.2 代码审查驱动的改进

在基本实现完成后，使用 AI 工具对 `external_sort.cpp` 进行了系统性的代码审查，发现了 8 个潜在问题（见附录）。经人工确认和分析后，对以下问题进行了手动修复：

1. **k=1 除零风险**：将 `k <= 0` 的判断改为 `k < 2`
2. **IntWriter 返回值被忽略**：在 `_Merge` 中增加了返回值检查
3. **`operator[]` 静默创建 map 条目**：改为 `at()` 进行只读访问
4. **`idx >= 0` 恒为真（无符号类型）**：移除无效条件
5. **文件打开无错误检查**：增加 `is_open()` 检查

其余问题（如 reader.cpp 中 IntReader 无法处理负数、临时文件未清理等）因涉及框架代码不可修改或在正常测试数据下不影响正确性而未做修改，但已在审查报告中记录。

---

## 七、AI 辅助使用说明

根据课程要求，声明如下：

- **`external_sort.cpp` 中的所有代码均为本人亲手编写**，没有任何代码通过 AI 工具直接生成。本人完全理解该文件中的每一行代码，并有能力进行详细讲解。
- 本项目的算法核心（置换-选择排序、最佳归并树、k 路归并）基于本人在《数据结构》课程中已实现的内存版外部排序原型（仓库：[DataStructure](https://github.com/SiriLee/DataStructure.git)，文件 `include/dsc/algorithm/external_sort.h`），经适配和优化后改造为本项目的文件 I/O 版本。
- AI 工具的使用限于以下两类辅助场景：
  1. **代码审查**：提交基本实现后，使用 AI 进行系统性检查，发现潜在缺陷；本人根据审查结果进行人工审视和手动修复。
  2. **文档编写**：使用 AI 辅助整理和撰写本实验报告。
- 完整的 AI 聊天记录见附录。

---

## 八、总结

本实验完成了一个完整的二阶段外部排序系统：

1. **第一阶段**采用置换-选择排序，利用最小堆和"冻结"机制，在内存限制为 k 的条件下生成尽可能长、尽可能少的初始有序归并段。
2. **第二阶段**结合最佳归并树策略和 k 路平衡归并，通过 Huffman 贪心选择最小段优先合并，最小化磁盘 I/O 开销，最终生成全局有序输出。

系统在 Linux (WSL2) 环境下通过了 N = 10,000,000 规模、k = 4 和 k = 64 的全部正确性验证。

---

## 附录 A：完整 AI 聊天记录（代码审查）

> 以下为使用 Claude Code 对 `external_sort.cpp` 进行代码审查的完整对话记录。

```

╭─── Claude Code v2.1.198 ─────────────────────────────────────────────────────╮
│                                         │ Tips for getting started           │
│              Welcome back!              │ Run /init to create a CLAUDE.md f… │
│                                         │ ────────────────────────────────── │
│                 ▐▛███▜▌                 │ What's new                         │
│                ▝▜█████▛▘                │ Claude in Chrome is now generally… │
│                  ▘▘ ▝▝                  │ Added background agent notificati… │
│                                         │ Added `/dataviz` skill for chart … │
│   deepseek-v4-pro · API Usage Billing   │ /release-notes for more            │
│   ~/workspace/projects/external-sort    │                                    │
╰──────────────────────────────────────────────────────────────────────────────╯


❯ 请帮我检查一下 @external_sort.cpp
  文件，查看是否存在潜在漏洞。若发现问题，需详细说明：内容、理由、具体依据。

  Thought for 3s (ctrl+o to expand)

● Let me first examine the header file and related files to understand the full
  context.

  Thought for 1m 56s, read 3 files, listed 1 directory (ctrl+o to expand)

● Now let me also check if there's a CLAUDE.md for project-specific
  instructions:

  Thought for 1s, read 1 file (ctrl+o to expand)

● 好的，我已经完整阅读了 external_sort.cpp、header.h、reader.cpp 和
  main.cpp，下面给出详细的代码审查报告。

  ---
  代码审查报告：external_sort.cpp

  共发现 8 个问题，按严重程度排序：

  ---
  🔴 1. k=1 时除零错误（未定义行为）— KWayMerge L135

  std::size_t d = (k - 1 - (n - 1) % (k - 1)) % (k - 1);

  问题：KWayMerge 仅在 L129 检查了 k <= 0，但没有检查 k == 1。当 k = 1 时，k - 1
  = 0，(n - 1) % 0 触发除零未定义行为（通常导致 SIGFPE 崩溃）。

  依据：C++ 标准 [expr.mul]/4：/ 和 % 的右操作数为零是未定义行为。

  建议修复：将 L129 改为 if (run_files.empty() || k < 2) return "";

  ---
  🔴 2. _Merge 中 IntWriter 返回值被忽略 — L110

  IntWriter(output, val); // Write the smallest value

  问题：IntWriter 返回 bool 表示写入成功与否（检查了 output.good()），但 _Merge
  中完全忽略了该返回值。如果磁盘满或发生写入错误，程序不会感知，继续运行，最终输
  出文件不完整却仍被当作正确结果返回。

  依据：reader.cpp L18-25 中 IntWriter
  明确设计为返回成功/失败状态，忽略它意味着错误被静默吞没。

  建议修复：
  if (!IntWriter(output, val)) {
      // Handle error: close streams, return empty string or throw
  }

  ---
  🔴 3. IntReader 无法处理负数 / 空白行导致数据丢失 — reader.cpp L27-47

  bool IntReader(std::ifstream &input, int &num) {
      if (input.eof()) { return false; }
      std::string s;
      std::getline(input, s);
      bool tag = true;
      for (auto c : s) {
          if (c >= '0' && c <= '9') {  // ← 仅识别 '0'-'9'
              tag = false;
              break;
          }
      }
      if (tag) { return false; }  // ← 无数字 → 直接返回 false
      ...
  }

  问题有两个：

  (a) 负数被静默丢弃：负数 -123 中没有字符 0-9（'-' 不匹配），tag 保持
  true，函数返回 false，该行数据被当作"无数据"丢弃。虽然当前 main.cpp
  生成的数据是非负的（distrib(0,
  1<<30)），但作为通用外部排序组件，这是严重的设计缺陷。

  (b) 空白行被当作 EOF：如果输入文件中间有空白行，IntReader 返回
  false，调用方（GenerateInitialRuns L74、_Merge
  L114）会停止从该流读取，导致后续数据丢失。

  依据：C++ 中 std::getline 可以读取空行（返回空字符串），但 IntReader 将其与
  EOF/错误等同处理。

  ---
  🟡 4. 文件打开操作无错误检查 — 多处

  涉及位置：
  - GenerateInitialRuns L63：current_run_output.open(current_run_file);
  - _Merge L97：inputs[i].open(run_files[to_merge[i]]);
  - _Merge L105：std::ofstream output(merged_file);
  - KWayMerge L138：std::ofstream dummy_output(dummy_run_file);

  问题：所有文件打开操作都没有检查是否成功。如果因权限不足、磁盘满、路径不存在等
  原因失败，后续的读写操作会静默失败：
  - GenerateInitialRuns 中若无法创建 run 文件，会陷入无限循环（is_open()
  始终返回 false，每次都尝试重新打开同一文件）
  - _Merge 中输入文件打开失败导致该 run 的数据全部丢失
  - 输出文件打开失败导致整个合并结果为空但函数正常返回

  依据：std::ofstream::open() 和 std::ifstream::open()
  不抛异常（默认），必须通过 is_open() 或 fail() 检查。当前代码除了 L54
  在关闭前检查 is_open()，从不检查打开是否成功。

  ---
  🟡 5. _data_count_cache 使用 operator[] 可能静默创建条目 — L149, L158

  // L149
  std::size_t data_count = _data_count_cache[_GetRunFileName(i)];

  // L158
  if (idx >= 0 && _data_count_cache[current_run_files[idx]] > 0) {

  问题：std::unordered_map::operator[] 在 key
  不存在时会插入一个默认值（0）的条目。如果由于某些原因某个 run 文件的 data
  count 没有被正确缓存（如逻辑变更、异常路径），使用 operator[] 会静默返回 0，该
  run 被当作空的 dummy run 跳过，导致数据静默丢失。

  依据：C++ 标准 [unord.map.elem]：operator[] 在 key 不存在时执行插入。应使用
  find() 或 at() 来区分"不存在"和"值为 0"。

  建议修复：使用 _data_count_cache.at(key) 或先 find() 检查。

  ---
  🟡 6. _Merge 中使用 run_files.size() 作为新文件名索引 — L104

  std::string merged_file = _GetRunFileName(run_files.size());

  问题：以 run_files 的当前大小作为新合并文件的索引，而 _GetRunFileName
  使用全局递增的索引。文件命名与 run_files
  的当前大小耦合，这在算法上碰巧正确（因为 run_files 是 current_run_files
  的引用且只增不减），但这种隐式依赖极易在重构中出错。如果将来 run_files
  中间有元素被移除，就会产生文件名冲突。

  依据：隐式契约（"文件名索引 =
  容器当前大小"）缺乏文档且脆弱。标准做法是维护一个显式的全局 run 计数器。

  ---
  🟢 7. 临时文件无清理机制 — 全局

  问题：GenerateInitialRuns 生成的中间 run 文件（run_0.txt, run_1.txt,
  ...）、dummy 文件、以及 _Merge 生成的中间合并文件，在最终结果产生后全部残留在
  save/ 目录下。KWayMerge 只在 L172 将最终合并文件重命名为输出文件，其余中间文件
  从不删除。多次运行会持续累积磁盘占用。

  依据：检查代码中无任何 std::filesystem::remove 调用。

  ---
  🟢 8. idx >= 0 检查恒为真 — L158

  if (idx >= 0 && _data_count_cache[current_run_files[idx]] > 0) {

  问题：idx 的类型是 std::size_t（无符号整数），idx >= 0 永远为
  true，属于无效代码。虽然不会导致错误，但表明编程者可能误以为 idx
  是有符号类型，掩盖了真正的意图（也许是检查 idx < current_run_files.size()
  防止越界？）。

  ---
  总结

  ┌─────────┬──────┬───────────────────────────────┬───────────────────┐
  │ 严重度  │ 编号 │             问题              │       位置        │
  ├─────────┼──────┼───────────────────────────────┼───────────────────┤
  │ 🔴 致命 │ 1    │ k=1 时除零 UB                 │ KWayMerge L135    │
  ├─────────┼──────┼───────────────────────────────┼───────────────────┤
  │ 🔴 致命 │ 2    │ IntWriter 返回值被忽略        │ _Merge L110       │
  ├─────────┼──────┼───────────────────────────────┼───────────────────┤
  │ 🔴 致命 │ 3    │ IntReader 无法处理负数/空白行 │ reader.cpp L34-42 │
  ├─────────┼──────┼───────────────────────────────┼───────────────────┤
  │ 🟡 中等 │ 4    │ 文件打开无错误检查            │ 多处              │
  ├─────────┼──────┼───────────────────────────────┼───────────────────┤
  │ 🟡 中等 │ 5    │ operator[] 静默创建 map 条目  │ L149, L158        │
  ├─────────┼──────┼───────────────────────────────┼───────────────────┤
  │ 🟡 中等 │ 6    │ 文件名索引隐式依赖容器 size   │ _Merge L104       │
  ├─────────┼──────┼───────────────────────────────┼───────────────────┤
  │ 🟢 轻微 │ 7    │ 临时文件未清理                │ 全局              │
  ├─────────┼──────┼───────────────────────────────┼───────────────────┤
  │ 🟢 轻微 │ 8    │ 无符号数与 0 比较恒为真       │ L158              │
  └─────────┴──────┴───────────────────────────────┴───────────────────┘

  其中问题 1（除零崩溃）和问题
  2（写入失败静默丢失）是可触发的最严重缺陷，建议优先修复。

✻ Cooked for 2m 34s

※ recap: 刚完成了 `external_sort.cpp` 的代码审查，发现 8 个问题：最严重的是 k=1 时除零崩溃，以及写入失败被静默忽略导致数据丢失。需要我开始修复这些问题吗？ (disable recaps in /config)

❯ 不考虑对当前文件以外的内容做任何修改，仅考虑待排序数据文件为正常数据时的情况
  ，可以参考 @main.cpp 。

  Thought for 1m 35s (ctrl+o to expand)

● 好的，我重新聚焦在 external_sort.cpp 本身，假设输入数据如 main.cpp
  生成的那样是正常的（非负整数、每行一个、无空行）。

  ---
  external_sort.cpp 代码审查报告（仅限本文件）

  ---
  🔴 问题 1：k=1 时除零 → 未定义行为（崩溃）

  位置：L135，KWayMerge 函数

  std::size_t d = (k - 1 - (n - 1) % (k - 1)) % (k - 1);

  内容：L129 只检查了 k <= 0，未检查 k == 1。当 k = 1 时，k - 1 = 0，表达式 (n -
  1) % 0 触发除零未定义行为（通常表现为 SIGFPE 信号，程序崩溃）。

  依据：C++ 标准 [expr.mul]/4 —— 乘法运算符 / 和 %
  的右操作数为零是未定义行为。main.cpp 中 TEST_KS = {4, 64} 不会触发，但
  KWayMerge 是公开 API（header.h L29-30 声明），调用方可传入任意 k。main.cpp
  L235 中 cli_k 来自 std::atoi(argv[2])，用户完全可能传入 1 导致崩溃。

  建议修复：
  if (run_files.empty() || k < 2) return "";

  ---
  🔴 问题 2：_Merge 忽略 IntWriter 返回值 —— 写入失败静默丢失

  位置：L110，_Merge 函数

  IntWriter(output, val); // Write the smallest value

  内容：IntWriter 通过 output.good() 检查写入状态并返回 bool，但 _Merge
  中完全忽略该返回值。如果发生磁盘满或其他写入错误，IntWriter 返回
  false，但合并循环继续执行，最终返回一个不完整/损坏的合并文件，且调用方
  KWayMerge 完全不知情。

  依据：reader.cpp L18-25 中 IntWriter
  明确设计为返回成功/失败状态，这意味着调用方应当检查。L69 GenerateInitialRuns
  中 IntWriter(current_run_output, value) 同样忽略了返回值。

  建议修复：检查返回值，失败时关闭流并提前返回空字符串或传播错误。

  if (!IntWriter(output, val)) {
      // 写入失败，关闭所有流，返回空字符串表示错误
      for (auto& in : inputs) if (in.is_open()) in.close();
      if (output.is_open()) output.close();
      return "";
  }

  ---
  🟡 问题 3：_Merge 输入文件打开无错误检查 —— 数据静默丢失

  位置：L97，_Merge 函数

  for (std::size_t i = 0; i < k; ++i) inputs[i].open(run_files[to_merge[i]]);

  内容：没有检查 inputs[i].is_open()。如果某个 run 文件不存在或被删除，open()
  静默失败，IntReader 会一直返回 false，导致该 run
  的全部数据丢失，合并结果不完整但函数正常返回。

  依据：std::ifstream::open() 默认不抛异常（未设置
  exceptions），必须主动检查。对比 L54 在关闭前检查了
  is_open()，证明开发者有意识做流状态检查，但打开时遗漏了。

  ---
  🟡 问题 4：_data_count_cache 用 operator[] 可能静默创建条目

  位置：L149, L158，KWayMerge 函数

  // L149
  std::size_t data_count = _data_count_cache[_GetRunFileName(i)];

  // L158
  if (idx >= 0 && _data_count_cache[current_run_files[idx]] > 0) {

  内容：std::unordered_map::operator[] 当 key 不存在时会插入默认值 0
  并返回。如果某个 run 文件的 data count 因逻辑错误未被缓存（比如
  GenerateInitialRuns 中提前返回导致部分 run 未记录），operator[] 会静默返回
  0，该 run 被当作 dummy run 跳过，数据静默丢失。

  依据：C++ 标准 [unord.map.elem] —— operator[] 在 key
  不存在时执行插入（value-initialized）。应使用 find() 或 at()
  区分"不存在"与"值为 0"。

  建议修复：
  // L149
  auto it = _data_count_cache.find(_GetRunFileName(i));
  if (it == _data_count_cache.end()) {
      // 错误：期望的 run 文件没有缓存记录
      return "";
  }
  std::size_t data_count = it->second;

  ---
  🟡 问题 5：GenerateInitialRuns 文件打开无错误检查 —— 潜在死循环

  位置：L63，GenerateInitialRuns 函数

  current_run_output.open(current_run_file);

  内容：如果文件打开失败（权限不足、磁盘满），is_open() 返回 false。下一次迭代中
  L61 if (!current_run_output.is_open())
  为真，会再次尝试打开同一文件，再次失败，形成无限循环——程序卡死。

  依据：与问题 3 同理，open() 不抛异常，必须检查返回值。

  ---
  🟢 问题 6：idx >= 0 检查恒为真（无效代码）

  位置：L158，KWayMerge 函数

  if (idx >= 0 && _data_count_cache[current_run_files[idx]] > 0) {

  内容：idx 声明为 std::size_t（无符号类型），idx >= 0
  永远为真，这个条件判断毫无作用。虽然不影响运行结果，但暗示开发者可能误认为 idx
  是有符号类型。真正应该检查的是 idx < current_run_files.size()（防越界）——当前
  依赖算法保证索引始终有效，缺乏防御性编程。

  依据：C++ 标准——无符号类型永远 ≥ 0，编译器通常产生 -Wtype-limits 警告。

  ---
  🟢 问题 7：临时文件永不清理

  位置：GenerateInitialRuns（创建 run
  文件）、_Merge（创建中间合并文件）、KWayMerge（创建 dummy 文件）

  内容：整个排序过程在 save/ 目录下生成大量中间文件（run_0.txt, run_1.txt,
  ...），但代码中没有任何 std::filesystem::remove 调用。KWayMerge L172
  仅将最终文件重命名为输出，中间文件全部残留。多次运行持续累积，浪费磁盘空间。

  依据：全文搜索 external_sort.cpp 中无任何 remove 或 delete 调用。

  ---
  总结

  ┌────────┬──────┬───────────────────────────────────────┬─────────────────┐
  │ 严重度 │ 编号 │                 问题                  │    触发条件     │
  ├────────┼──────┼───────────────────────────────────────┼─────────────────┤
  │ 🔴     │ 1    │ k=1 除零未定义行为                    │ 调用方传入 k=1  │
  │ 致命   │      │                                       │                 │
  ├────────┼──────┼───────────────────────────────────────┼─────────────────┤
  │ 🔴     │ 2    │ IntWriter 返回值被忽略                │ 磁盘满/写入错误 │
  │ 致命   │      │                                       │                 │
  ├────────┼──────┼───────────────────────────────────────┼─────────────────┤
  │ 🟡     │ 3    │ _Merge 输入文件打开无错误检查         │ 中间文件丢失    │
  │ 中等   │      │                                       │                 │
  ├────────┼──────┼───────────────────────────────────────┼─────────────────┤
  │ 🟡     │ 4    │ operator[] 静默创建 map 条目          │ 缓存缺失        │
  │ 中等   │      │                                       │                 │
  ├────────┼──────┼───────────────────────────────────────┼─────────────────┤
  │ 🟡     │ 5    │ GenerateInitialRuns                   │ 目录不可写      │
  │ 中等   │      │ 打开文件无错误检查                    │                 │
  ├────────┼──────┼───────────────────────────────────────┼─────────────────┤
  │ 🟢     │ 6    │ idx >= 0 无效判断                     │ 始终存在        │
  │ 轻微   │      │                                       │                 │
  ├────────┼──────┼───────────────────────────────────────┼─────────────────┤
  │ 🟢     │ 7    │ 临时文件未清理                        │ 始终存在        │
  │ 轻微   │      │                                       │                 │
  └────────┴──────┴───────────────────────────────────────┴─────────────────┘

  最优先修复：问题 1（一行改动即可避免崩溃）和问题 2（数据完整性致命缺陷）。

✻ Crunched for 2m 4s
```

---

## 附录 B：AI 聊天记录（文档编写）

> 文档编写部分的 AI 辅助过程为本轮对话，此处从略。具体内容即本文档本身——AI 根据项目 README、源代码、Git 提交历史和本人提供的补充说明，协助将以上信息组织为结构化的实验报告。
