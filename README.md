# OpenTextProjector

- [Design goals](#design-goals)
- [Platforms](#platforms)
- [Installation and Contribution](#installantion-and-contribution)
- [Documentation](#documentation)
- [Example](#example)
- [Used third-party tools/libraries](#used-third-party-tools/libraries)
- [License](#license)

## Design goals

There are lots of programs to project text on screen out there, but none offered the possibility to control the monitor remotely (at least none that I could find). This program lets you show text on the screen of a computer via TCP-Socket. The program gets a JSON obect with information about the text, and the monitor and it just shows the desired text on the desired monitor. I had this project idea for church, when I had to show verses and lyrics while doing a live streaming. This project gives you the posibility to share the work with someone else as they can change the lyrics/verses via phone. 

**ATTENTION**: This project is only the backend, meaning it has no user interface, it just shows the text on the screen. I am currently working on a web intreface for this project. 

The goal of this project is to have a program that can show text on screen and be able to take commands from a TCP socket, also it is desired to have a RTSP/RTMP server to stream the text for a streaming software (like OBS).

## Platforms

Currently OpenTextProjector only works on Windows (tested on Windows 10), but other plytforms are on the way (specifically MacOS and Linux).

## Installation and Contribution

To install this project you need to download the release and double click on the .exe file. If it doesn't work you may need to change the graphics card under which the program is run. For example if you hava an NVIDIA graphics card, you need to go to the NVIDIA control panel and under ```3D Settings``` select ```Manage 3D settings```, then switch to the ```Program Settings``` tab and click on add and select the executable ```OpenTextProjector.exe```. After that select your NVIDIA graphics processor as your preferred graphics processor.

Any contribution is welcome. I've made this project as easy to build as it can get, you only have to open the project in Visual Studio (2022). Steps you have to follow:
1. Download and install Visual Studio Comunity Version (2022) from the Microsoft webpage
2. In the installer select "Desktop development with C++" then click install
3. Now you should be able to at least build the project.
4. If it's running, but you can't see any text on screen, change the graphics card as explaind above for the following file: ```/path/to/OpenTextPojector/Debug/OpenTextProjector.exe```

## Documentation

This program listens for a TCP Socket connection on port 8080, and takes a JSON object with the commands. 

Here are all the possible commands:

- ```text``` - ```string``` text that should be shown on the screen encoded with ```base64```, empty text for blank screen.
- ```monitor``` - ```int``` the index of the monitor, starting with 0 (1 for secondary monitor, 2 for the third and so on).
- ```font_size``` - ```int``` size in pixels of the font.
- ```font``` - ```string``` name of the ```.ttf``` file that must be present in the ```fonts``` folder.
- ```font_color``` - ```int array``` of size 3, with the first element representing the color RED (between 0.0 and 1.0), the second element representing the color GREEN (between 0.0 and 1.0), the third representing BLUE (between 0.0 and 1.0). You can use a combination of those to achieve your desired color (RGB).

Any command can be left out completly. If one command is missing, the previously command will be taken (for example if ```font_color``` is missing, the last color that has been set will be set)

## Example

Here's a JSON object example:

```
{
  "text": "SGVsbG8gV29ybGQ=",
  "monitor": 0,
  "font_size": 35,
  "font": "JandaAppleCobbler",
  "font_color": [
    1,
    0,
    0
  ]
}
```

## Used third-party tools/libraries

- [freetype](https://freetype.org/)
- [json](https://github.com/nlohmann/json)
- [tinythread](https://sourceforge.net/projects/tinythread/)
- [cpp-base64](https://github.com/ReneNyffenegger/cpp-base64)
- [glew](https://github.com/nigels-com/glew)
- [glfw](https://www.glfw.org/)

## License

See the ```LICENSE``` file