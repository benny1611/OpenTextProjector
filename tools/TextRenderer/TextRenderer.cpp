#include "TextRenderer.h"

#include <freetype/ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <freetype/fttrigon.h>
#include FT_FREETYPE_H
#include <GL/glew.h>

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <locale>
#include <codecvt>
#include <map>

// All ideas here ripped from a NEHE tutorial
// http://nehe.gamedev.net/tutorial/freetype_fonts_in_opengl/24001/

namespace glfreetype {
    
    int numberOfCharacters = 512;

    std::map<GLuint, int> texturesAndSizes; // textures with their respective sizes

    unsigned short* correctedString(std::wstring text, int& index, int& withOfTheText, bool debug = false);
    
    // Gets the first power of 2 >= 
    // for the given int  
    inline int next_p2(int a)
    {
        int rval = 1;
        // rval<<=1 Is A Prettier Way Of Writing rval*=2;
        while (rval < a) rval <<= 1;
        return rval;
    }

    FT_BitmapGlyph generateBitmapForFace(FT_Face face, GLuint ch) {

        // Load the character glyph.
        unsigned int index = FT_Get_Char_Index(face, ch);
        //std::cout << "Generating: " << ch << " " << (char)ch << " index: " << index << std::endl;
        if (FT_Load_Glyph(face, index, FT_LOAD_DEFAULT))
            throw std::runtime_error("FT_Load_Glyph failed");

        // Move into a glyph object
        FT_Glyph glyph;
        if (FT_Get_Glyph(face->glyph, &glyph))
            throw std::runtime_error("FT_Get_Glyph failed");

        // Convert to a bitmap
        FT_Glyph_To_Bitmap(&glyph, ft_render_mode_normal, 0, 1);
        return (FT_BitmapGlyph)glyph;
    }

    void storeTextureData(int const width,
        int const height,
        FT_Bitmap& bitmap,
        std::vector<GLubyte>& expanded_data)
    {
        // Note: two channel bitmap (One for
        // channel luminosity and one for alpha).
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                expanded_data[2 * (i + j * width)] = 255; // luminosity
                expanded_data[2 * (i + j * width) + 1] =
                    (i >= bitmap.width || j >= bitmap.rows) ? 0 :
                    bitmap.buffer[i + bitmap.width * j];
            }
        }
    }

    // Create A Display List Corresponding To The Given Character.
    void make_dlist(FT_Face face, GLuint ch, GLuint list_base, GLuint* tex_base) {

        // Retrieve a bitmap for the given char glyph.
        FT_BitmapGlyph bitmap_glyph = generateBitmapForFace(face, ch);

        // This Reference Will Make Accessing The Bitmap Easier.
        FT_Bitmap& bitmap = bitmap_glyph->bitmap;

        // Get correct dimensions for bitmap
        int width = next_p2(bitmap.width);
        int height = next_p2(bitmap.rows);

        texturesAndSizes.insert({ tex_base[ch], bitmap.width });

        // Use a vector to store texture data (better than a raw array).
        std::vector<GLubyte> expanded_data(2 * width * height, 0);

        // Does what it says
        storeTextureData(width, height, bitmap, expanded_data);

        // Texture parameters.
        glBindTexture(GL_TEXTURE_2D, tex_base[ch]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        // Create the texture Itself
        // GL_LUMINANCE_ALPHA to indicate 2 channel data.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
            GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, &expanded_data.front());

        // Create the display list
        glNewList(list_base + ch, GL_COMPILE);

        glBindTexture(GL_TEXTURE_2D, tex_base[ch]);

        glPushMatrix();

        // Ensure right amount of space for each new char
        glTranslatef(bitmap_glyph->left, 0, 0);
        glTranslatef(0, bitmap_glyph->top - (bitmap.rows * 0.9), 0);

        // Account for padding.
        float x = (float)bitmap.width / (float)width,
            y = (float)bitmap.rows / (float)height;

        // Draw the texturemapped quads.
        glBegin(GL_QUADS);
        glTexCoord2d(0, 0); glVertex2f(0, bitmap.rows);
        glTexCoord2d(0, y); glVertex2f(0, 0);
        glTexCoord2d(x, y); glVertex2f(bitmap.width, 0);
        glTexCoord2d(x, 0); glVertex2f(bitmap.width, bitmap.rows);
        glEnd();
        glPopMatrix();

        // Move over for next character
        glTranslatef(face->glyph->advance.x >> 6, 0, 0);

        //glBitmap(0, 0, 0, 0, face->glyph->advance.x >> 6, 0, NULL);

        // Finish The Display List
        glEndList();
    }

    void font_data::init(const char* fname, unsigned int h) {

        // Allocate Some Memory To Store The Texture Ids.
        textures.resize(512);

        this->h = h;

        // Create And Initilize A FreeType Font Library.
        FT_Library library;
        if (FT_Init_FreeType(&library))
            throw std::runtime_error("FT_Init_FreeType failed");

        // The Object In Which FreeType Holds Information On A Given
        // Font Is Called A "face".
        FT_Face face;

        // This Is Where We Load In The Font Information From The File.
        // Of All The Places Where The Code Might Die, This Is The Most Likely,
        // As FT_New_Face Will Fail If The Font File Does Not Exist Or Is Somehow Broken.
        if (FT_New_Face(library, fname, 0, &face))
            throw std::runtime_error("FT_New_Face failed (there is probably a problem with your font file)");

        // For Some Twisted Reason, FreeType Measures Font Size
        // In Terms Of 1/64ths Of Pixels.  Thus, To Make A Font
        // h Pixels High, We Need To Request A Size Of h*64.
        // (h << 6 Is Just A Prettier Way Of Writing h*64)
        FT_Set_Char_Size(face, h << 6, h << 6, 96, 96);

        // Here We Ask OpenGL To Allocate Resources For
        // All The Textures And Display Lists Which We
        // Are About To Create. 
        list_base = glGenLists(512);
        glGenTextures(512, &textures.front());

        // This Is Where We Actually Create Each Of The Fonts Display Lists.
        for (GLuint i = 0;i < 512;i++) {
            make_dlist(face, i, list_base, &textures.front());
        }

        // We Don't Need The Face Information Now That The Display
        // Lists Have Been Created, So We Free The Assosiated Resources.
        FT_Done_Face(face);

        // Ditto For The Font Library.
        FT_Done_FreeType(library);
    }

    void font_data::clean() {
        glDeleteLists(list_base, 512);
        glDeleteTextures(512, &textures.front());
    }

    // A Fairly Straightforward Function That Pushes
    // A Projection Matrix That Will Make Object World
    // Coordinates Identical To Window Coordinates.
    inline void pushScreenCoordinateMatrix() {
        glPushAttrib(GL_TRANSFORM_BIT);
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(viewport[0], viewport[2], viewport[1], viewport[3]);
        glPopAttrib();
    }

    // Pops The Projection Matrix Without Changing The Current
    // MatrixMode.
    inline void pop_projection_matrix() {
        glPushAttrib(GL_TRANSFORM_BIT);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopAttrib();
    }

    // Much Like NeHe's glPrint Function, But Modified To Work
    // With FreeType Fonts.
    void print(const font_data& ft_font, float x, float y, std::vector<std::wstring> text) {

        // We Want A Coordinate System Where Distance Is Measured In Window Pixels.
        pushScreenCoordinateMatrix();

        GLuint font = ft_font.list_base;
        // We Make The Height A Little Bigger.  There Will Be Some Space Between Lines.
        float h = ft_font.h / .63f;

        // Split text into lines
        glPushAttrib(GL_LIST_BIT | GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TRANSFORM_BIT);
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glListBase(font);

        float modelview_matrix[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, modelview_matrix);

        // This Is Where The Text Display Actually Happens.
        // For Each Line Of Text We Reset The Modelview Matrix
        // So That The Line's Text Will Start In The Correct Position.
        // Notice That We Need To Reset The Matrix, Rather Than Just Translating
        // Down By h. This Is Because When Each Character Is
        // Drawn It Modifies The Current Matrix So That The Next Character
        // Will Be Drawn Immediately After It. 
        for (int i = 0; i < text.size(); i++) {

            int length;
            int withOfTheText;
            unsigned short* correctedChars = correctedString(text[i], length, withOfTheText);

            glPushMatrix();
            glLoadIdentity();
            glTranslatef(x - (withOfTheText/2), y - h * i, 0);
            glMultMatrixf(modelview_matrix);

            // The Commented Out Raster Position Stuff Can Be Useful If You Need To
            // Know The Length Of The Text That You Are Creating.
            // If You Decide To Use It Make Sure To Also Uncomment The glBitmap Command
            // In make_dlist().
            glCallLists(length, GL_UNSIGNED_SHORT, correctedChars);

            glPopMatrix();
            delete[] correctedChars;
        }

        glPopAttrib();

        pop_projection_matrix();
    }

    unsigned short* correctedString(std::wstring text, int& index, int& withOfTheText, bool debug) {
        unsigned short* all_chars = new unsigned short[text.length()];
        index = 0;
        withOfTheText = 0;
        if (debug) {
            std::cout << "Showing following characters:" << std::endl;
        }
        for (int j = 0; j < text.length(); j++) {
            if (debug) {
                std::cout << "Character: " << text[j] << std::endl;
            }
            if (text[j] == 196) {
                bool skip = true;
                switch (text[j + 1]) {
                case 402:
                    all_chars[index] = 0x103;
                    withOfTheText += texturesAndSizes.find(0x61)->second; // same width as "a"
                    break;
                case 8218:
                    all_chars[index] = 0x102;
                    withOfTheText += texturesAndSizes.find(0x41)->second; // same width as "A"
                    break;
                }
                if (skip) {
                    j++;
                }
            } else if (text[j] == 200) {
                bool skip = true;
                switch (text[j + 1]) {
                case 8482:
                    all_chars[index] = 0x15F;
                    withOfTheText += texturesAndSizes.find(0x73)->second; // same width as "s"
                    break;
                case 8250:
                    all_chars[index] = 0x163;
                    withOfTheText += texturesAndSizes.find(0x74)->second; // same width as "t"
                    break;
                case 732:
                    all_chars[index] = 0x15E;
                    withOfTheText += texturesAndSizes.find(0x53)->second; // same width as "S"
                    break;
                case 353:
                    all_chars[index] = 0x162;
                    withOfTheText += texturesAndSizes.find(0x54)->second; // same width as "T"
                    break;
                default:
                    skip = false;
                }
                if (skip) {
                    j++;
                }
            } else if (text[j] == 195) {
                bool skip = true;
                switch (text[j + 1]) {
                case 174:
                    all_chars[index] = 0xEE;
                    withOfTheText += texturesAndSizes.find(0x69)->second; // same width as "i"
                    break;
                case 162:
                    all_chars[index] = 0xE2;
                    withOfTheText += texturesAndSizes.find(0x61)->second; // same width as "a"
                    break;
                case 8218:
                    all_chars[index] = 0xC2;
                    withOfTheText += texturesAndSizes.find(0x41)->second; // same width as "A"
                    break;
                case 381:
                    all_chars[index] = 0xCE;
                    withOfTheText += texturesAndSizes.find(0x49)->second; // same width as "I"
                    break;
                case 8211:
                    all_chars[index] = 0xD6;
                    withOfTheText += texturesAndSizes.find(0x4F)->second; // same width as "O"
                    break;
                case 182:
                    all_chars[index] = 0xF6;
                    withOfTheText += texturesAndSizes.find(0x6F)->second; // same width as "o"
                    break;
                case 8222:
                    all_chars[index] = 0xC4;
                    withOfTheText += texturesAndSizes.find(0x41)->second; // same width as "A"
                    break;
                case 164:
                    all_chars[index] = 0xE4;
                    withOfTheText += texturesAndSizes.find(0x61)->second; // same width as "a"
                    break;
                case 339:
                    all_chars[index] = 0xDC;
                    withOfTheText += texturesAndSizes.find(0x55)->second; // same width as "U"
                    break;
                case 188:
                    all_chars[index] = 0xFC;
                    withOfTheText += texturesAndSizes.find(0x75)->second; // same width as "u"
                    break;
                default:
                    skip = false;
                }
                if (skip) {
                    j++;
                }
            }
            else {
                all_chars[index] = (unsigned short)text[j];
                withOfTheText += texturesAndSizes.find(text[j])->second;
            }
            index++;
        }
        if (debug) {
            std::cout << "--------- DONE ---------" << std::endl;
        }
        return all_chars;
    }
}