# APISan: Sanitizing API Usages through Semantic Cross-Checking

APISAN is a tool that automatically infers correct API usages from source code without manual effort. The key idea in APISAN is to extract likely correct usage patterns in four different aspects (e.g., causal relation, and semantic relation on arguments) by considering semantic constraints. APISAN is tailored to check various properties with security implications. We applied APISAN to 92 million lines of code, including Linux Kernel, and OpenSSL, found 76 previously unknown bugs, and provided patches for all the bugs.

This repository has analysis tool and LLVM. LLVM related files follow their own license(LICENSE.LLVM), and analysis tool is provided under the terms of the MIT license.

## How to use
- Tested in Ubuntu 14.04
- Setup
```sh
  $ ./setup.sh
```
- How to build symbolic database
```sh
  $ apisan build [cmds]
```
- Run './configure'
```sh
  $ apisan build ./configure
  $ apisan build make
```
- How to run a checker
```sh
  $ apisan check --db=[db] --checker=[checker]
```
- Example
```sh
  $ cd test/return-value
  $ ../../apisan build make
  $ ../../apisan check --checker=rvchk
```

## Checkers (under analyzer/apisan/check)
- Return value checker: retval.py
- Argument checker: argument.py
- Causality checker: causality.py
- Condition checker: condition.py
- Integer overflow checker: intovfl.py
- Format string bug checker: fsb.py

## Authors
- Insu Yun <insu@gatech.edu>
- Changwoo Min <changwoo@gatech.edu>
- Xujie Si <six@gatech.edu>
- Yeongjin Jang <yeongjin.jang@gatech.edu> 
- Taesoo Kim <taesoo@gatech.edu>
- Mayur Naik <naik@cc.gatech.edu>

## Publications
```
@inproceedings{yun:apisan,
  title        = {{APISan: Sanitizing API Usages through Semantic Cross-checking}},
  author       = {Insu Yun and Changwoo Min and Xujie Si and Yeongjin Jang and Taesoo Kim and Mayur Naik},
  booktitle    = {Proceedings of the 25th USENIX Security Symposium (Security)},
  month        = aug,
  year         = 2016,
  address      = {Austin, TX},
}
```
