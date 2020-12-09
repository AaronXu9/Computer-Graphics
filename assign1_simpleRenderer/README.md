# Height Fields Using Shaders #

## An Overview ##
Height fields may be found in many applications of computer graphics. They are used to represent terrain in video games and simulations, and also often utilized to represent data in three dimensions. This assignment asks you to create a height field based on the data from an image which the user specifies at the command line, and to allow the user to manipulate the height field in three dimensions by rotating, translating, or scaling it. You also have to implement a vertex shader that performs smoothing of the geometry, and re-adjusts the geometry color. After the completion of your program, you will use it to create an animation. You will program the assignment using OpenGL's core profile.

This assignment is intended as a hands-on introduction to OpenGL and programming in three dimensions. It teaches the OpenGL's core profile and shader-based programming. The provided starter gives the functionality to initialize GLUT, read and write a JPEG image, handle mouse and keyboard input, and display one triangle to the screen. You must write code to handle camera transformations, transform the landscape (translate/rotate/scale), and render the heightfield. You must also write a shader to perform geometry smoothing and re-color the terrain accordingly. Please see the OpenGL Programming Guide for information, or OpenGL.org.

## Background Information ##
A height field is a visual representation of a function which takes as input a two-dimensional point and returns a scalar value ("height") as output.

Rendering a height field over arbitrary coordinates is somewhat tricky--we will simplify the problem by making our function piece-wise. Visually, the domain of our function is a two-dimensional grid of points, and a height value is defined at each point. We can render this data using only a point at each defined value, or use it to approximate a surface by connecting the points with triangles in 3D.

You will be using image data from a grayscale JPEG file to create your height field, such that the two dimensions of the grid correspond to the two dimensions of the image and the height value is a function of the image grayscale level. Since you will be working with grayscale image, the bytes per pixel (i.e., ImageIO::getBytesPerPixel) is always 1 and you don't have to worry about the case where the bytes per pixel is 3 (i.e., RGB images).

Render Points, Lines and Triangles
Your program needs to render the height field as points (when the key "1" is pressed on the keyboard), lines ("wireframe"; key "2"), or solid triangles (key "3"). The points, lines and solid triangles must be modeled using GL_POINTS, GL_LINES, GL_TRIANGLES, or their "LOOP" or "STRIP" variants. Usage of glPolygonMode (or similar) to achieve point or line rendering is not permitted. If in doubt, please ask the instructor/TA.

## Render Points, Lines and Triangles ##
Your program needs to render the height field as points (when the key "1" is pressed on the keyboard), lines ("wireframe"; key "2"), or solid triangles (key "3"). The points, lines and solid triangles must be modeled using GL_POINTS, GL_LINES, GL_TRIANGLES, or their "LOOP" or "STRIP" variants. Usage of glPolygonMode (or similar) to achieve point or line rendering is not permitted. If in doubt, please ask the instructor/TA.

## Vertex Shader Requirement ## 

You should write a vertex shader that provides two rendering modes. In the first mode, simply transform the vertex with the modelview and projection matrix. Leave the color unchanged. This vertex shader is already provided in the starter code. This mode should be used for point rendering (key "1"), line rendering (key "2"), and triangle rendering (key "3").

In the second mode (key "4"), change the vertex position to the average position of the four neighboring vertices in the vertex shader. Specifically, replace p_center with (p_left + p_right + p_down + p_up) / 4 (see image). This will have the effect of smoothening the terrain (the effect is most visible at low image resolutions, e.g., 128 x 128). Then transform the resulting vertex position with the modelview and projection matrix in the vertex shader as usual.

!()[https://github.com/AaronXu9/Computer-Graphics/blob/main/assign1_simpleRenderer/a1_images/a1_image1.png]

The positions of the four neighboring vertices should be passed into the vertex shader as additional attributes, in the same way as vertex position and color. In order to accommodate this, you should create additional VBOs. For example, one approach is to create 4 VBOs of 3-floats: position of the left vertex, right vertex, up vertex, down vertex. Note that these positions include the height of the vertex as one of its coordinates.

You should write one vertex shader that satisfies the above requirements. In order to switch between the two modes, you should create a uniform variable "uniform int mode" in the vertex shader, and set its value using the OpenGL function glUniform1i on the CPU, as follows. When the user presses keys "1", "2" or "3", mode should be set to 0, and when the user presses key "4", mode should be set to 1. When mode=0, the vertex shader should execute the first mode described above. When mode=1, the vertex shader should execute the second mode (smoothen the vertex position). You can achieve this using an "if" statement in the vertex shader. When mode=1, you should also correct the color in the vertex shader to correspond to the smoothened height, using the formula:


