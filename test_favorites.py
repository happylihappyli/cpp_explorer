import struct

# 创建一个测试用的favorites.dat文件
def create_test_favorites():
    # 收藏夹数量
    favorite_count = 2
    
    # 收藏夹数据
    favorites = [
        ("桌面", "C:\\Users\\Public\\Desktop"),
        ("文档", "C:\\Users\\Public\\Documents")
    ]
    
    # 写入文件
    with open("favorites.dat", "wb") as f:
        # 写入收藏夹数量
        f.write(struct.pack('i', favorite_count))
        
        # 写入每个收藏夹项
        for name, path in favorites:
            # 写入名称长度和名称
            name_bytes = name.encode('utf-16le')
            f.write(struct.pack('i', len(name)))
            f.write(name_bytes)
            
            # 写入路径长度和路径
            path_bytes = path.encode('utf-16le')
            f.write(struct.pack('i', len(path)))
            f.write(path_bytes)
    
    print("已创建测试用的favorites.dat文件")

if __name__ == "__main__":
    create_test_favorites()