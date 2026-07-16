# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0

"""
ESP-IDF HelloWorld示例项目的自动化测试脚本

该模块包含以下测试用例：
1. test_hello_world: 在实际ESP硬件上运行的通用测试
2. test_hello_world_linux: 在Linux主机上运行的测试
3. test_hello_world_host: 在QEMU模拟器上运行的测试，包含ELF文件SHA256校验

测试框架依赖：
- pytest: Python测试框架
- pytest-embedded-idf: ESP-IDF嵌入式测试插件
- pytest-embedded-qemu: QEMU模拟器测试插件

使用方法：
    pytest hello_world.py -p no:warnings --target <target>
"""

import hashlib              # 用于计算文件SHA256哈希值
import logging              # 用于日志记录
from typing import Callable # 类型注解，用于函数参数类型提示

import pytest
from pytest_embedded_idf.dut import IdfDut      # ESP-IDF设备测试对象
from pytest_embedded_qemu.app import QemuApp    # QEMU应用对象
from pytest_embedded_qemu.dut import QemuDut    # QEMU设备测试对象


@pytest.mark.supported_targets    # 标记支持所有官方支持的ESP目标芯片
@pytest.mark.preview_targets      # 标记支持预览版目标芯片
@pytest.mark.generic              # 标记为通用测试用例
def test_hello_world(
    dut: IdfDut, log_minimum_free_heap_size: Callable[..., None]
) -> None:
    """
    HelloWorld项目的通用硬件测试用例

    测试步骤：
    1. 等待设备输出"Hello world!"字符串，验证程序正常启动
    2. 调用log_minimum_free_heap_size函数记录最小空闲堆内存信息

    参数：
        dut: IdfDut类型，代表连接的ESP设备测试对象，用于发送命令和接收输出
        log_minimum_free_heap_size: 回调函数，用于记录并输出系统最小空闲堆内存大小
    """
    # 等待设备输出"Hello world!"，验证应用程序成功启动
    dut.expect('Hello world!')
    # 记录最小空闲堆内存信息，用于内存使用分析
    log_minimum_free_heap_size()


@pytest.mark.linux      # 标记为Linux主机测试
@pytest.mark.host_test  # 标记为主机测试用例
def test_hello_world_linux(dut: IdfDut) -> None:
    """
    HelloWorld项目在Linux主机上的测试用例

    测试步骤：
    1. 等待设备输出"Hello world!"字符串，验证Linux版本程序正常运行

    参数：
        dut: IdfDut类型，代表Linux主机上运行的测试对象
    """
    # 等待程序输出"Hello world!"，验证Linux版本应用程序成功启动
    dut.expect('Hello world!')


def verify_elf_sha256_embedding(app: QemuApp, sha256_reported: str) -> None:
    """
    验证ELF文件的SHA256哈希值是否与应用程序报告的一致

    该函数用于确保QEMU模拟器中运行的ELF文件与编译生成的文件完全一致，
    防止文件损坏或版本不匹配。

    参数：
        app: QemuApp类型，包含QEMU应用程序信息，提供ELF文件路径
        sha256_reported: 字符串类型，应用程序报告的SHA256哈希值前缀

    返回：
        None

    异常：
        ValueError: 当计算的SHA256与报告的SHA256不匹配时抛出
    """
    # 创建SHA256哈希对象
    sha256 = hashlib.sha256()
    
    # 以二进制模式打开ELF文件并计算哈希值
    with open(app.elf_file, 'rb') as f:
        sha256.update(f.read())
    
    # 获取完整的SHA256十六进制字符串
    sha256_expected = sha256.hexdigest()

    # 记录日志，输出计算的和应用程序报告的SHA256值
    logging.info(f'ELF file SHA256: {sha256_expected}')
    logging.info(f'ELF file SHA256 (reported by the app): {sha256_reported}')

    # 应用程序只报告SHA256的前若干位，检查是否匹配
    if not sha256_expected.startswith(sha256_reported):
        raise ValueError('ELF file SHA256 mismatch')


@pytest.mark.esp32       # 标记为ESP32芯片测试（当前仅ESP32支持QEMU）
@pytest.mark.host_test   # 标记为主机测试用例
@pytest.mark.qemu        # 标记为QEMU模拟器测试
def test_hello_world_host(app: QemuApp, dut: QemuDut) -> None:
    """
    HelloWorld项目在QEMU模拟器上的测试用例

    测试步骤：
    1. 从设备输出中提取ELF文件的SHA256哈希值前缀
    2. 调用verify_elf_sha256_embedding验证ELF文件完整性
    3. 等待设备输出"Hello world!"，验证程序正常运行

    参数：
        app: QemuApp类型，包含QEMU应用程序信息，提供ELF文件路径
        dut: QemuDut类型，代表QEMU模拟器中的设备测试对象
    """
    # 使用正则表达式从设备输出中提取SHA256哈希值前缀
    sha256_reported = (
        dut.expect(r'ELF file SHA256:\s+([a-f0-9]+)').group(1).decode('utf-8')
    )
    
    # 验证ELF文件的SHA256哈希值是否与报告一致
    verify_elf_sha256_embedding(app, sha256_reported)

    # 等待设备输出"Hello world!"，验证QEMU中程序成功运行
    dut.expect('Hello world!')