## UE4 Plugin: HotUpdate
[HotUpdate](https://github.com/crazytuzi/HotUpdate)是一个用于UE4资源热更新下载的插件，经过测试，UMG，地图和Lua都成功热更新，理论上其他uasset资源都可以，PC，Android和IOS都测试通过。目前处于测试阶段，会持续更新，欢迎提issus。制作Pak部分推荐[HotPatcher](https://github.com/hxhb/HotPatcher)。

- 已知问题
    - IOS下加载metalmap和metallib当前方案是临时方案

- 首先在Project Settings - Plugins - HotUpdate下设置参数
    <br>
    <img src="Settings.png" width="1320">
    - HotUpdateServerUrl : 热更新服务器地址
        - 注意，需要带上http://或者https://，否则会发生IOS请求Http失败
    - TempPakSaveRoot : 临时下载文件保存目录
    - PakSaveRoot : Pak保存目录
    - TimeOutDelay : 尝试重连间隔时间
    - MaxRetryTime : 尝试重连最大次数
- 然后配置Http服务器
    <br>
    <img src="WWW.png" width="442">
    - index.php为热更新入口，根据客户端通过Json格式传入的版本号和平台，返回对应的热更新资源Json。这里版本号可以对接项目自身CICD实现自动化。
        ```php
        <?php
        // index.php
        ini_set("display_errors", "On");

        ini_set("error_reporting",E_ALL);

        header('Content-Type:application/json; charset=utf-8');

        $data = json_decode(file_get_contents('php://input'));

        $version = $data->{"version"};

        $platform = $data->{"platform"};

        $file = $version . "/" . $platform ."/". "version.json";

        if(file_exists($file))
        {
            $json_contents = file_get_contents($file);

            echo $json_contents;
        }
        else
        {
            exit(json_encode(""));
        }
        ?>
        ```
    - 平台对应文件夹列表，可根据项目需求在源码中进行修改
        - PLATFORM_DESKTOP &&  WITH_EDITOR - editor
        - PLATFORM_WINDOWS - win
        - PLATFORM_ANDROID - android
        - PLATFORM_IOS - ios
        - PLATFORM_MAC - mac
        - PLATFORM_LINUX - linux
    - version.json中为版本对应热更新资源列表，可根据项目自身CICD实现自动化生成。
        ```json
        {
            "0.1.0.0" : [
            ],
            "0.1.1.0" : [
                {
                    "File": "0.1.1.0_Asset_WindowsNoEditor_001_P.pak",
                    "HASH": "806f2c9f104b03552145a14063648126",
                    "Size": 15116689
                },
                {
                    "File": "0.1.1.0_Lua_WindowsNoEditor_001_P.pak",
                    "HASH": "483c3342a912bf177058f0979ea52ee5",
                    "Size": 681
                }
            ]
        }
        ```
- 最后只需要在项目适当位置运行UHotUpdateSubsystem的StartUp函数即可
    <br>
    <img src="Startup.png" width="1001">
- 相关事件列表
    - OnDownloadUpdate 下载进度
    - OnMountUpdate Mount进度
    - OnHotUpdateFinished 热更新完成

- IOS下metalmap和metallib加载说明
    - 首先是制作Pak包的时候，需要将metalmap和metallib放到其他非Content目录，例如Content/Metal
        - 目录设置在HotPatcher插件中FlibPatchParserHelper.cpp中，如下图
            <br>
            <img src="FlibPatchParserHelper.png" width="2069">
    - 然后是需要修改引擎MetalShaders.cpp文件
        - 主要修改代码如下图
            <br>
            <img src="MetalShaders.png" width="1147">
        - 流程分析
            - 首先检测文件名是否包含"Metal"，"Metal"为FlibPatchParserHelper.cpp中配置
            - 如果包含，代表这是需要热更新的metallib，执行下面代码，否则不做处理
            - 创建一个文件，此文件加了一层Lib目录，此时文件的路径为Content/Metal/Lib，这样做的目的是方便获取绝对路径
            - 将Pak中的metallib文件内容拷贝到新建的文件中，这是目前的临时方案，会有比较大的IO开销，后期会考略结合newLibraryWithData使用文件流
    - 最后修改插件FilePakManager.cpp中的路径，如下图
        <br>
        <img src="FilePakManager.png" width="1431">
    - MetalShaders目录下有原始和修改后MetalShaders.cpp文件
        - 引擎版本4.25.1