# 主题命名与迁移

| 旧 ID / 名称 | 新 ID / 名称 | 理由 |
| --- | --- | --- |
| dark / 深色 | nightfall / 长夜 | 用夜间陪伴的时间意象表达深色 |
| light / 浅色 | daylight / 白昼 | 对应日间清晰开放的使用环境 |
| contrast / 高对比 | boundary / 断界 | 以清晰边界表达高对比；正文 AAA |
| moon / 月下 | moon / 月轨 | 银蓝轨道与时序世界观关联 |
| suit / 白橙 | suit / 特行 | 表达行动与专业工具感 |

读取同一 settings.json 的 theme 时自动迁移并写回。其他设置原样保留；moon/suit 不改 ID。跟随系统和高级自定义配色仍可使用；五个预设通过 tools/theme-tokens.json 定义语义令牌，与五风格完全解耦。自定义配色由用户选择，预设的自动对比度保证不适用于任意自选色。
