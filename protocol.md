UI请求打开文件：
```json
{
    cmd: "openFile",
    id: "ui-1",
}
```

打开成功：
```json
{
    cmd: "reply",
    id: "ui-1",
    success: true
}
```

打开失败：
```json
{
    cmd: "reply",
    id: "ui-1",
    success: false,
    why: "文件打开失败"
}
```

打开成功，返回promise:
```json
{
    cmd: "reply",
    id: "ui-1",
    success: true,
    promise: true
}
```

ui查询Promise进度
```json
{
    cmd: "queryPromise",
    id: "ui-2"
}
```

处理中
```json
{
    cmd: "reply",
    id: "ui-2",
    success: true,
    process: 50
}

处理完成
```json
{
    cmd: "reply",
    id: "ui-2",
    success: true,
    process: 100
}