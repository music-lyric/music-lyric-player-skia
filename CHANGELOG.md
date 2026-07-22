# Changelog

## v0.1.0 (2026-07-23)

### Feature

- bump lyric model version ([9310cb2](https://github.com/music-lyric/music-lyric-player-skia/commit/9310cb2f99389477074adf9b99de4cf009864404), [6001972](https://github.com/music-lyric/music-lyric-player-skia/commit/6001972b35af2d7ca4f074c6715140e5eb3dd20e), [d0e63ba](https://github.com/music-lyric/music-lyric-player-skia/commit/d0e63baf95d529b3e408134f613782fc3a20b3b5), [38d33e8](https://github.com/music-lyric/music-lyric-player-skia/commit/38d33e85708eb2a8f8a39a70266df684efaeae6b), [d2040cc](https://github.com/music-lyric/music-lyric-player-skia/commit/d2040ccb6bc58f1381b348553aac1282218133cc))
- serialize config enums by name ([803a958](https://github.com/music-lyric/music-lyric-player-skia/commit/803a958caf395bdab36fd8ec291b3dfe0a359786))
- support enum config fields ([305e3e2](https://github.com/music-lyric/music-lyric-player-skia/commit/305e3e2c14cdfeed3616b8d17cdf417a9e8efaeb))
- add default config ([9e55946](https://github.com/music-lyric/music-lyric-player-skia/commit/9e5594601360312177a1d470801e616351143454))
- `backend`
  - embed icu data ([c956a7f](https://github.com/music-lyric/music-lyric-player-skia/commit/c956a7f09cece6f99098631358d1d89f66fc1ebb))
  - add font manager provider ([c31fc11](https://github.com/music-lyric/music-lyric-player-skia/commit/c31fc11f2f28b6e1a109475699c646f373878467))
  - add gpu surface ([4c621e9](https://github.com/music-lyric/music-lyric-player-skia/commit/4c621e9ca354d4873a8ede4391de131aff326f74))
- `example`
  - support load lyric ([8f42043](https://github.com/music-lyric/music-lyric-player-skia/commit/8f420431568cf1fcbc8975fa8d0c47e6e3231c3e))
- `platform`
  - expose config update on native abi facade ([6204e46](https://github.com/music-lyric/music-lyric-player-skia/commit/6204e4621943c5cfa8d687500605b4e862af9f4a))
  - add config update to native abi ([7a071ec](https://github.com/music-lyric/music-lyric-player-skia/commit/7a071ec633f818f8d66250c689178b165d5e99e8))
  - add native abi ([067e8e5](https://github.com/music-lyric/music-lyric-player-skia/commit/067e8e5e7840b871740dba4b8974ce98ac5f61f6))
- `playback`
  - add config manager with schema codegen ([65b08b7](https://github.com/music-lyric/music-lyric-player-skia/commit/65b08b79a7aefa3543a384db3a70512410fb3377))
  - add player ([ba2b602](https://github.com/music-lyric/music-lyric-player-skia/commit/ba2b602d4e9d9c17ae2fbad3493786c9587a63d9))
- `rendering`
  - add container edge fade ([0c27496](https://github.com/music-lyric/music-lyric-player-skia/commit/0c2749606e88d4007f045e1894592b8f4a4aae6d))
  - split state color from mask alpha reveal ([1685498](https://github.com/music-lyric/music-lyric-player-skia/commit/16854986040a14a45cacddf2af7a7231a074a3ee))
  - reverse syllable float on line exit ([9c1f3b1](https://github.com/music-lyric/music-lyric-player-skia/commit/9c1f3b18c75a167772fe7bb0f8daf6492ff408f1))
  - add syllable word float animation ([1db5350](https://github.com/music-lyric/music-lyric-player-skia/commit/1db535006300c25d586053200f3947b0a051e4f0))
  - add syllable word mask animation ([83c0337](https://github.com/music-lyric/music-lyric-player-skia/commit/83c033751765e81b01fef4a5a6e3ac1ab58ce552))
  - add syllable line element ([ea2b3ca](https://github.com/music-lyric/music-lyric-player-skia/commit/ea2b3ca0c7ae15811ba0be7c6e638d4d9fed5f15))
  - add line interlude animation ([71763a5](https://github.com/music-lyric/music-lyric-player-skia/commit/71763a5c8eff79d4f44cfd05238c3de7ba47a68a))
  - add gaussian focus effect ([a1099c5](https://github.com/music-lyric/music-lyric-player-skia/commit/a1099c522494c535b75a97f595e6a51b8418d890))
  - add cascade scroll animation modes ([e7163e2](https://github.com/music-lyric/music-lyric-player-skia/commit/e7163e204b095773def60712bec52ab3327d32f5))
  - ease line state color per frame ([fafc9cd](https://github.com/music-lyric/music-lyric-player-skia/commit/fafc9cd2def6384a36e4716d275ac4bf2e2b0eda))
  - add line manager ([b1856d6](https://github.com/music-lyric/music-lyric-player-skia/commit/b1856d65f9aeaf9b725dfce9f3ab7bd5b0877dee))
  - add layout and scroll managers ([9717402](https://github.com/music-lyric/music-lyric-player-skia/commit/9717402233b3545eef72e9a10bf4432039b74f81))
  - add interlude line component ([b6332b4](https://github.com/music-lyric/music-lyric-player-skia/commit/b6332b4d1f4654b1e8b941f0849f9708562157a7))
  - add animation engine ([e2bae84](https://github.com/music-lyric/music-lyric-player-skia/commit/e2bae841d40248a2deaa14c24d76decd81aa4122))
  - add new config item ([4e7fe20](https://github.com/music-lyric/music-lyric-player-skia/commit/4e7fe20a4e6c57c62482dd7bfcd5e8f3f8dd8e68))
  - ease line scrolling ([62265b8](https://github.com/music-lyric/music-lyric-player-skia/commit/62265b8b2542a6324498e27b901a9d6d79891fe0))
  - add base ([fed98d9](https://github.com/music-lyric/music-lyric-player-skia/commit/fed98d9cbda00b412c68807f9fd059a3f8e99134))
- `utils`
  - update config manager class name ([c342939](https://github.com/music-lyric/music-lyric-player-skia/commit/c3429399dfda4bc66623cd66684c05584814bfab))
  - add tools ([afaf853](https://github.com/music-lyric/music-lyric-player-skia/commit/afaf853e62381d71bfa63641cdcd5c4fa964fee9))

### Fix

- `platform`
  - complete font manager type in native abi ([df93969](https://github.com/music-lyric/music-lyric-player-skia/commit/df939696e2c57461c373f56a6703a05c1061ca26))
- `playback`
  - correct caret version check for zero-major releases ([dffe67a](https://github.com/music-lyric/music-lyric-player-skia/commit/dffe67ac4155f082356b7e4c461d49c997b47f09))
  - read lyric offset from meta ([15fa49f](https://github.com/music-lyric/music-lyric-player-skia/commit/15fa49f5337ce4ded436b810fa08429063b245ec))
- `rendering`
  - wrap complex script lines ([9e5220a](https://github.com/music-lyric/music-lyric-player-skia/commit/9e5220a2d93fb0f6bd705d26e7b53fe507cd432c))
  - improve syllable mask rendering ([53411d5](https://github.com/music-lyric/music-lyric-player-skia/commit/53411d5e5b6400e54a2701e115426b931af3433a))
  - prevent normal line word wrapping ([6a4be87](https://github.com/music-lyric/music-lyric-player-skia/commit/6a4be87ccf842ae723c6717e6ae004688bd120d8))

### Refactor

- replace dot-path diff with change struct in config ([90a891c](https://github.com/music-lyric/music-lyric-player-skia/commit/90a891c4461917e07396a012dfaa6502f11e22d0))
- restructure config schema metadata ([ac340d7](https://github.com/music-lyric/music-lyric-player-skia/commit/ac340d7f3b43b1b33ec7dcdce1605ed5a29e0ef6))
- extract common config ([982d974](https://github.com/music-lyric/music-lyric-player-skia/commit/982d97455f32544a9bcd201fe90347b57fd5f988))
- unify member access convention ([1958af6](https://github.com/music-lyric/music-lyric-player-skia/commit/1958af6e72ab47de6af1186083af10013b40dd3b))
- rename directory ([c58ee3c](https://github.com/music-lyric/music-lyric-player-skia/commit/c58ee3caa8679f5d6e29d8065679bfb35053cd7b))
- `example`
  - render through backend surface ([8da8708](https://github.com/music-lyric/music-lyric-player-skia/commit/8da8708323465dd5d84a65d4eb8ec69cfb455f52))
- `playback`
  - restructure config namespace ([79f7947](https://github.com/music-lyric/music-lyric-player-skia/commit/79f7947dbae33ef47ac532c45db5d4f518482456))
  - rename directory ([dc5eb13](https://github.com/music-lyric/music-lyric-player-skia/commit/dc5eb1310037472941cffc9fe33e36ae99dc363f))
  - drop local lyric access in favor of model helpers ([37a66ff](https://github.com/music-lyric/music-lyric-player-skia/commit/37a66ffd15fd4d53b267c8fa07b7c8f20130a233))
- `rendering`
  - extract frame background and padding into container component ([c2d6d61](https://github.com/music-lyric/music-lyric-player-skia/commit/c2d6d611625a9e29ad0838f02f3d04231dfb208d))
  - rename namespace ([7dff0b2](https://github.com/music-lyric/music-lyric-player-skia/commit/7dff0b2a7f85756d37084b8bb30a9c64a202e077))
  - self-shape plain line content ([918999e](https://github.com/music-lyric/music-lyric-player-skia/commit/918999e812c5c79cc82a4ed5657cfa8d5d28ac4c))
  - move line normal main content ([205d249](https://github.com/music-lyric/music-lyric-player-skia/commit/205d249d8c95192b81d95debd877d03e13e4242e))
  - restructure config schema ([5ea92d7](https://github.com/music-lyric/music-lyric-player-skia/commit/5ea92d7496781b1a322a253f6bdbb835556459fe))
  - split main content style from line base ([2761147](https://github.com/music-lyric/music-lyric-player-skia/commit/2761147f1bdfc0d9465ffb5a423095d6d8aecb91))
  - nest line base appearance under normal ([35664b6](https://github.com/music-lyric/music-lyric-player-skia/commit/35664b6a49b7072051ec7760c1106f9347f0e831))
  - line word layout ([0c22030](https://github.com/music-lyric/music-lyric-player-skia/commit/0c220304b32f916afe9d027e38faaa2d5ebac6f7))
  - extract plain line element ([7f27769](https://github.com/music-lyric/music-lyric-player-skia/commit/7f27769495906754f85e954585134aea38318faa))
  - group animation timing config by scroll mode ([b671d77](https://github.com/music-lyric/music-lyric-player-skia/commit/b671d77fab6a7d5c3ff04c8c10a90946170cfb34))
  - accept color as string in config ([8023e32](https://github.com/music-lyric/music-lyric-player-skia/commit/8023e32d016325e274ddeca70971346a2a823537))
  - hoist line alignment into base element ([a9a7780](https://github.com/music-lyric/music-lyric-player-skia/commit/a9a77803cf99fb25fdeaac9c92ad97a92c8fea5c))
  - extract line component ([32aa8a5](https://github.com/music-lyric/music-lyric-player-skia/commit/32aa8a5b5c6f28ceb4d16448fab93729e226a5d8))
- `utils`
  - nest shared types in namespace ([95ddee2](https://github.com/music-lyric/music-lyric-player-skia/commit/95ddee28f20bf142e3c214ac86c8f97c322cd2d0))
  - config manager ([91edc6d](https://github.com/music-lyric/music-lyric-player-skia/commit/91edc6d67096667b35969d7cc3f578f7b5e67246))

### Document

- optimize comment ([d41ec71](https://github.com/music-lyric/music-lyric-player-skia/commit/d41ec71f7525904e88fb10a5068c79a1491b2117))
