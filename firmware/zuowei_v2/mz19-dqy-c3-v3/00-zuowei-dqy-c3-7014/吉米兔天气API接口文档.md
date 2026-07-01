# 天气 API 接口文档


## 接口地址

```
http://8.166.128.230:3100/api/weather?key=YOUR_KEY&mac=DEVICE_MAC
```

---

## 请求参数

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `key` | string | 是 | API Key，用于鉴权 |
| `mac` | string | 是 | 设备 MAC 地址，格式 `XX:XX:XX:XX:XX:XX`（半角冒号分隔，大小写均可） |

**请求示例：**

```http
GET /api/weather?key=YOUR_KEY&mac=ac:a7:04:10:4a:a0
```

---

## 成功响应（200）

```json
{
  "results": [
    {
      "location": {
        "id": "101280110",
        "name": "白云",
        "country": "CN",
        "path": "白云,广东省,中国",
        "timezone": "Asia/Shanghai",
        "timezone_offset": "+08:00"
      },
      "now": {
        "text": "小雨",
        "code": "13",
        "temperature": "27"
      },
      "last_update": "2026-05-21T08:02:59.988Z"
    }
  ]
}
```

### 响应字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `results[].location.id` | string | 城市 ID |
| `results[].location.name` | string | 城市名称 |
| `results[].location.path` | string | 省市完整路径 |
| `results[].now.text` | string | 天气现象中文 |
| `results[].now.code` | string | 天气现象代码 |
| `results[].now.temperature` | string | 当前温度（摄氏度） |
| `results[].last_update` | string | 数据更新时间（ISO 8601） |

---

## 天气现象代码对照表

| 代码 | 天气 | 代码 | 天气 |
|:----:|------|:----:|------|
| 0 | 晴（白天） | 20 | 雨夹雪 |
| 1 | 晴（夜间） | 21 | 阵雪 |
| 2 | 晴 | 22 | 小雪 |
| 3 | 晴 | 23 | 中雪 |
| 4 | 多云 | 24 | 大雪 |
| 5 | 晴间多云 | 25 | 暴雪 |
| 6 | 晴间多云 | 26 | 浮尘 |
| 7 | 大部多云 | 27 | 扬沙 |
| 8 | 大部多云 | 28 | 沙尘暴 |
| 9 | 阴 | 29 | 强沙尘暴 |
| 10 | 阵雨 | 30 | 雾 |
| 11 | 雷阵雨 | 31 | 霾 |
| 12 | 雷阵雨伴有冰雹 | 32 | 风 |
| 13 | 小雨 | 33 | 大风 |
| 14 | 中雨 | 34 | 飓风 |
| 15 | 大雨 | 35 | 热带风暴 |
| 16 | 暴雨 | 36 | 龙卷风 |
| 17 | 大暴雨 | 37 | 冷 |
| 18 | 特大暴雨 | 38 | 热 |
| 19 | 冻雨 | 99 | 未知 |

---

## 错误响应

| HTTP 状态码 | error | 说明 |
|:-----------:|-------|------|
| 400 | `INVALID_MAC` | MAC 地址格式无效 |
| 401 | `MISSING_KEY` | 缺少 API Key 参数 |
| 401 | `INVALID_KEY` | Key 不存在 / 已禁用 / 已过期 |
| 503 | `WEATHER_ERROR` | 天气服务暂不可用 |

**错误响应格式：**

```json
{
  "code": 401,
  "error": "INVALID_KEY",
  "message": "key已禁用"
}
```

---

## SDK 调用示例

### cURL

```bash
curl "http://8.166.128.230:3100/api/weather?key=YOUR_KEY&mac=ac:a7:04:10:4a:a0"
```

---

## 注意事项

1. **城市定位** — 系统通过 MAC 地址实时定位设备所在城市，无需传 IP 或城市名
2. **缓存策略** — 天气数据缓存 10 分钟，城市 ID 缓存 1 小时
3. **Key 保管** — 请勿泄露 Key，如需更多 Key 联系管理员
