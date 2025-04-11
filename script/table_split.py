import psycopg2

# 连接到 PostgreSQL 数据库
conn = psycopg2.connect(
    database="postgres",
    user="postgres",
    password="password",
    host="localhost",
    port="5433"
)
cur = conn.cursor()

try:
    # 为 device 字段创建索引
    create_index_query = "CREATE INDEX IF NOT EXISTS idx_device ON postgres (device);"
    print(f"Creating index with SQL: {create_index_query}")
    cur.execute(create_index_query)

    # 提前获取除 device 列之外的所有列名和类型
    cur.execute("""
        SELECT 
            string_agg(quote_ident(attname) || ' ' || format_type(atttypid, atttypmod), ', '),
            string_agg(quote_ident(attname), ', ')
        FROM pg_attribute
        WHERE attrelid = 'postgres'::regclass
          AND attnum > 0
          AND NOT attisdropped
          AND attname <> 'device';
    """)
    column_list, non_device_columns = cur.fetchone()

    # 获取不同的 device 值
    cur.execute("SELECT DISTINCT device FROM postgres")
    devices = cur.fetchall()

    for device in devices:
        device_value = device[0]
        print(f"Processing device: {device_value}")

        # 构建创建表的 SQL 语句
        create_table_query = f"CREATE TABLE IF NOT EXISTS {device_value} ({column_list})"
        print(f"Creating table with SQL: {create_table_query}")
        cur.execute(create_table_query)

        # 构建插入数据的 SQL 语句
        insert_query = f"INSERT INTO {device_value} SELECT {non_device_columns} FROM postgres WHERE device = '{device_value}'"
        print(f"Inserting data with SQL: {insert_query}")
        cur.execute(insert_query)

    # 提交事务
    conn.commit()
    print("All operations completed successfully.")

except (Exception, psycopg2.Error) as error:
    print(f"Error: {error}")
    # 回滚事务
    conn.rollback()

finally:
    # 关闭游标和连接
    if cur:
        cur.close()
    if conn:
        conn.close()