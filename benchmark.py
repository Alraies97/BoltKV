import asyncio
import time

async def send_command(writer, reader, cmd):
    writer.write(f"{cmd}\n".encode())
    await writer.drain()
    response = await reader.readline()
    return response.decode().strip()

async def worker(total_requests, worker_id):
    reader, writer = await asyncio.open_connection('127.0.0.1', 6380)
    
    # قياس عمليات الكتابة (SET)
    start_set = time.time()
    for i in range(total_requests):
        await send_command(writer, reader, f"SET bench:{worker_id}:{i} value_data_123")
    end_set = time.time()
    
    # قياس عمليات القراءة (GET)
    start_get = time.time()
    for i in range(total_requests):
        await send_command(writer, reader, f"GET bench:{worker_id}:{i}")
    end_get = time.time()
    
    writer.close()
    await writer.wait_closed()
    
    return end_set - start_set, end_get - start_get

async def main():
    num_workers = 50
    requests_per_worker = 5000
    total_requests = num_workers * requests_per_worker
    
    print(f"⚡ Starting BoltKV Benchmark: {total_requests} total operations...")
    
    start_time = time.time()
    tasks = [worker(requests_per_worker, i) for i in range(num_workers)]
    results = await asyncio.gather(*tasks)
    total_time = time.time() - start_time
    
    total_set_time = sum(r[0] for r in results) / num_workers
    total_get_time = sum(r[1] for r in results) / num_workers
    
    qps = total_requests / total_time
    avg_latency = (total_time / total_requests) * 1000 # بالملي ثانية
    
    print("\n📊 --- BENCHMARK RESULTS ---")
    print(f"🔹 Total Throughput: {qps:.2f} QPS (Queries Per Second)")
    print(f"🔹 Avg Latency per Request: {avg_latency:.4f} ms")
    print(f"🔹 Total execution time for {total_requests} ops: {total_time:.3f} seconds")

if __name__ == "__main__":
    asyncio.run(main())