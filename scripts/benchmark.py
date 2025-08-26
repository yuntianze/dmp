#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DMP 系统性能基准测试
目标: P99 ≤ 50ms, 吞吐量 ≥ 10,000 TPS
"""

import asyncio
import aiohttp
import json
import time
import statistics
from typing import List, Dict
import argparse

class DMPBenchmark:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url
        self.results = []
    
    def generate_test_request(self, request_id: int) -> Dict:
        """生成测试请求数据"""
        return {
            "request_id": f"test_{request_id:06d}",
            "timestamp": int(time.time() * 1000),
            "transaction": {
                "amount": 1299.99,
                "currency": "USD",
                "merchant_id": f"MERCH_{request_id % 1000:05d}",
                "merchant_category": "5411",
                "pos_entry_mode": "CHIP"
            },
            "card": {
                "token": f"tok_{request_id:016d}",
                "issuer_country": "US",
                "card_brand": "VISA"
            },
            "device": {
                "ip": f"192.168.1.{request_id % 254 + 1}",
                "fingerprint": f"df_{request_id:08d}",
                "user_agent": "Mozilla/5.0 (Test Client)"
            },
            "customer": {
                "id": f"cust_{request_id % 10000}",
                "risk_score": 35,
                "account_age_days": 365
            }
        }
    
    async def send_request(self, session: aiohttp.ClientSession, request_data: Dict) -> float:
        """发送单个请求并返回延迟"""
        start_time = time.perf_counter()
        try:
            async with session.post(f"{self.base_url}/api/v1/decision", 
                                   json=request_data,
                                   timeout=aiohttp.ClientTimeout(total=5)) as response:
                await response.read()
                latency = (time.perf_counter() - start_time) * 1000  # ms
                return latency
        except Exception as e:
            print(f"请求失败: {e}")
            return -1
    
    async def run_benchmark(self, total_requests: int, concurrency: int):
        """运行性能测试"""
        print(f"🚀 开始性能测试")
        print(f"📊 总请求数: {total_requests}")
        print(f"⚡ 并发数: {concurrency}")
        
        connector = aiohttp.TCPConnector(limit=concurrency)
        async with aiohttp.ClientSession(connector=connector) as session:
            # 预热
            print("🔥 预热阶段...")
            warmup_tasks = []
            for i in range(min(100, total_requests)):
                request_data = self.generate_test_request(i)
                task = self.send_request(session, request_data)
                warmup_tasks.append(task)
            
            await asyncio.gather(*warmup_tasks)
            
            # 正式测试
            print("📈 正式测试...")
            start_time = time.time()
            
            tasks = []
            for i in range(total_requests):
                request_data = self.generate_test_request(i)
                task = self.send_request(session, request_data)
                tasks.append(task)
                
                # 控制并发数
                if len(tasks) >= concurrency:
                    batch_results = await asyncio.gather(*tasks)
                    self.results.extend([r for r in batch_results if r > 0])
                    tasks = []
            
            # 处理剩余任务
            if tasks:
                batch_results = await asyncio.gather(*tasks)
                self.results.extend([r for r in batch_results if r > 0])
            
            total_time = time.time() - start_time
        
        self.analyze_results(total_time)
    
    def analyze_results(self, total_time: float):
        """分析测试结果"""
        if not self.results:
            print("❌ 没有成功的请求")
            return
        
        success_count = len(self.results)
        qps = success_count / total_time
        
        # 延迟统计
        p50 = statistics.median(self.results)
        p95 = statistics.quantiles(self.results, n=20)[18]  # 95th percentile
        p99 = statistics.quantiles(self.results, n=100)[98]  # 99th percentile
        avg = statistics.mean(self.results)
        
        print(f"\n📊 测试结果:")
        print(f"✅ 成功请求: {success_count}")
        print(f"⚡ QPS: {qps:.1f}")
        print(f"📈 延迟统计 (ms):")
        print(f"   平均: {avg:.2f}")
        print(f"   P50:  {p50:.2f}")
        print(f"   P95:  {p95:.2f}")
        print(f"   P99:  {p99:.2f}")
        
        # SLO 检查
        print(f"\n🎯 SLO 达成情况:")
        print(f"   P99 ≤ 50ms: {'✅' if p99 <= 50 else '❌'} ({p99:.2f}ms)")
        print(f"   QPS ≥ 10K:  {'✅' if qps >= 10000 else '❌'} ({qps:.0f})")

async def main():
    parser = argparse.ArgumentParser(description="DMP 性能基准测试")
    parser.add_argument("--url", default="http://localhost:8080", help="服务器地址")
    parser.add_argument("--requests", type=int, default=10000, help="总请求数")
    parser.add_argument("--concurrency", type=int, default=100, help="并发数")
    
    args = parser.parse_args()
    
    benchmark = DMPBenchmark(args.url)
    await benchmark.run_benchmark(args.requests, args.concurrency)

if __name__ == "__main__":
    asyncio.run(main())
