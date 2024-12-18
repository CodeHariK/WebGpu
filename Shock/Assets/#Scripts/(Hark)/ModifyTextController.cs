/*
テストです。[wave6.0]波打たせたり[-wave][circle6.0]回したり[-circle][colorful]カラフルにしたり[-colorful]できるよ

How to use ModifyTextController

1. Add ModifyText - ModifyTextController to UI Text
 (You can also add [AddComponent]-[UI]-[Effects]-[Custom]-[ModifyText]))
2. Fill in the tagged text
3. End

[About tags]

The tag recognizes commands sandwiched between [].
If you add -(minus) to the command, it will return to normal. (e.g., [-wave], [-shake])

%[#ffffffff]
Change the color.
The ffffffff part is a color code.

・[defcolor]
Undoes the color.

・[colorful]
Colorful characters.

・[shake2.0]
Shake at a width of 2.0.
You can change the part of 2.0 freely.

・[wave2.0]
Wave with a width of 2.0.
You can change the part of 2.0 freely.

・[circle2.0]
Turn around at a width of 2.0
You can change the part of 2.0 freely.

・[scale2.0]
2.0 times the size.
You can change the part of 2.0 freely.

・[scalex2.0]
Make the x direction 2.0 times larger.
You can change the part of 2.0 freely.

・[scaley2.0]
Make the y-direction 2.0 times larger.
You can change the part of 2.0 freely.

・[rot2.0]
Rotates the characters at a speed of 2.0 (rad).
You can change the part of 2.0 freely.

・[random2.0]
Even the width of 2.0 moves.
You can change the part of 2.0 freely.

・[randscale2.0]
Randomly increases or becomes smaller at a width of 2.0.
You can change the part of 2.0 freely.

・[small2.0]
After 2.0 seconds, it gradually becomes smaller.
You can change the part of 2.0 freely.

・[fadeout2.0]
After 2.0 seconds, it gradually disappears.
You can change the part of 2.0 freely.

・[fadein]
Fade in.

・[flash2.0]
Flashes every 2.0 seconds.
You can change the part of 2.0 freely.

・[default]
Undo changes made by all tags.


【Example sentences】
- It is a test. [wave6.0] you can make it wave[-wave] [circle6.0] turn, [-circle] [colorful] colorful, [-colorful]
- It is a test. [shake4.0] [#ffaa00] rattling [default] or [scale1.2] bigger [scale0.7] smaller [default], [#ffffff44] [wave3.0] translucency [default] of course you can


[Known bugs]
Position is wrong. Position well.
- Depending on the font, it will not be displayed correctly.
*/

using System.Collections.Generic;
using UnityEngine.UI;
using UnityEngine;
using System.Text.RegularExpressions;
using System;

[AddComponentMenu("UI/Effects/Custom/ModifyText")]
public class ModifyTextController : BaseMeshEffect
{
    public float Interval = 0.06f;
    public float TextSpeed = 20.0f;
    public float WaveSpeed = 3.0f;
    public float CircleSpeed = 3.0f;
    public float CharacterSpacing = 0.0f;
    public float LineSpacing = 6.0f;

    public bool isEnd = false;


    float time = 0.0f;
    float totaltime = 0.0f;
    int charCount;
    float alpha;

    Graphic g;
    Text text;
    string srcText;

    class Char
    {
        public int vertIndex;
        public char text;
        public float shakeRadius;
        public float waveDist;
        public float circleRadius;
        public Vector3 scale;
        public Color32 color;
        public bool colorful;
        public float rot;
        public float random;
        public float randscale;
        public float small;
        public float fadeout;
        public bool fadein;
        public float flash;
    }

    public event EventHandler TextFinished = delegate { };

    protected override void Start()
    {
        base.Start();
        g = GetComponent<Graphic>();
        text = GetComponent<Text>();
    }

    public void Init()
    {
        time = 0.0f;
        totaltime = 0.0f;
        charCount = 0;
        isEnd = false;
    }

    public override void ModifyMesh(VertexHelper vh)
    {
        if (!IsActive() || IsDestroyed()) return;
        if (string.IsNullOrEmpty(srcText)) return;

        List<UIVertex> vertices = new List<UIVertex>();
        vh.GetUIVertexStream(vertices);

        var chars = ExtractText(srcText, ref vertices);

        ModifyText(ref vertices, chars);

        TextAlpha(ref vertices);

        vh.Clear();
        vh.AddUIVertexTriangleStream(vertices);
    }

    static Regex re = new Regex(@"[^0-9.]");
    Char[] ExtractText(string src, ref List<UIVertex> vertices)
    {
        float shakeRadius = 0.0f;
        float waveDist = 0.0f;
        float circleRadius = 0.0f;
        Color color = text.color;
        bool colorful = false;
        var scale = Vector3.one;
        var rot = 0.0f;
        var rand = 0.0f;
        var randscale = 0.0f;
        var small = 0.0f;
        var fadeout = 0.0f;
        var fadein = false;
        var flash = 0.0f;

        var chars = new List<Char>();
        for (var i = 0; i < src.Length; i++)
        {
            while (src[i] == '[')
            {
                var end = i;
                while (src[end] != ']') end++;

                var tag = src.Substring(i + 1, end - i - 1);
                i = end + 1;

                if (tag.Contains("#"))
                {
                    ColorUtility.TryParseHtmlString(tag, out color);
                }
                else if (tag.Contains("-shake"))
                {
                    shakeRadius = 0.0f;
                }
                else if (tag.Contains("shake"))
                {
                    shakeRadius = float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("-wave"))
                {
                    waveDist = 0.0f;
                }
                else if (tag.Contains("wave"))
                {
                    waveDist = float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("-circle"))
                {
                    circleRadius = 0.0f;
                }
                else if (tag.Contains("circle"))
                {
                    circleRadius = float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("-colorful"))
                {
                    colorful = false;
                }
                else if (tag.Contains("colorful"))
                {
                    colorful = true;
                }
                else if (tag.Contains("-randscale"))
                {
                    randscale = 0.0f;
                }
                else if (tag.Contains("randscale"))
                {
                    randscale = float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("-scalex"))
                {
                    scale = new Vector3(1.0f, scale.y, scale.z);
                }
                else if (tag.Contains("scalex"))
                {
                    scale = new Vector3(float.Parse(re.Replace(tag, "")), scale.y, scale.z);
                }
                else if (tag.Contains("-scaley"))
                {
                    scale = new Vector3(scale.x, 1.0f, scale.z);
                }
                else if (tag.Contains("scaley"))
                {
                    scale = new Vector3(scale.x, float.Parse(re.Replace(tag, "")), scale.z);
                }
                else if (tag.Contains("-scale"))
                {
                    scale = Vector3.one;
                }
                else if (tag.Contains("scale"))
                {
                    scale = Vector3.one * float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("-rot"))
                {
                    rot = 0.0f;
                }
                else if (tag.Contains("rot"))
                {
                    rot = float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("-random"))
                {
                    rand = 0.0f;
                }
                else if (tag.Contains("random"))
                {
                    rand = float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("-small"))
                {
                    small = 0.0f;
                }
                else if (tag.Contains("small"))
                {
                    small = float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("-fadein"))
                {
                    fadein = false;
                }
                else if (tag.Contains("fadein"))
                {
                    fadein = true;
                }
                else if (tag.Contains("-fadeout"))
                {
                    fadeout = 0.0f;
                }
                else if (tag.Contains("fadeout"))
                {
                    fadeout = float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("-flash"))
                {
                    flash = 0.0f;
                }
                else if (tag.Contains("flash"))
                {
                    flash = float.Parse(re.Replace(tag, ""));
                }
                else if (tag.Contains("defcolor"))
                {
                    color = text.color;
                }
                else if (tag.Contains("default"))
                {
                    shakeRadius = 0.0f;
                    waveDist = 0.0f;
                    circleRadius = 0.0f;
                    color = text.color;
                    colorful = false;
                    scale = Vector3.one;
                    rot = 0.0f;
                    rand = 0.0f;
                    randscale = 0.0f;
                    small = 0.0f;
                    fadeout = 0.0f;
                    fadein = false;
                    flash = 0.0f;
                }

                if (i >= src.Length) break;
            }
            if (i >= src.Length) break;

            chars.Add(new Char()
            {
                vertIndex = i,
                text = src[i],
                shakeRadius = shakeRadius,
                waveDist = waveDist,
                circleRadius = circleRadius,
                color = color,
                scale = scale,
                colorful = colorful,
                rot = rot,
                random = rand,
                randscale = randscale,
                small = small,
                fadeout = fadeout,
                fadein = fadein,
                flash = flash,
            });
        }

        var vs = new List<UIVertex>();
        for (var i = 0; i < chars.Count; i++)
        {
            for (int c = 0; c < 6; c++) vs.Add(vertices[chars[i].vertIndex * 6 + c]);
        }

        Vector3 startPos = vertices[0].position;
        float nowX = startPos.x;
        int vertical = 0;
        for (var i = 0; i < chars.Count; i++)
        {
            Vector3 pivot = vs[i * 6].position;
            var w = vs[i * 6 + 1].position.x - vs[i * 6 + 0].position.x;
            for (int c = 0; c < 6; c++)
            {
                var v = vs[i * 6 + c];
                var dt = vs[i * 6 + c].position - pivot;
                var dh = startPos.y - pivot.y;
                v.position = dt + new Vector3(nowX, startPos.y - dh - LineSpacing * vertical, 0.0f);
                vs[i * 6 + c] = v;
            }

            if (chars[i].text != '\n')
            {
                nowX += w + CharacterSpacing;
            }
            else
            {
                vertical++;
                nowX = startPos.x;
            }
        }

        vertices = vs;

        return chars.ToArray();
    }

    void ModifyText(ref List<UIVertex> vertices, Char[] chars)
    {
        for (int i = 0; i < chars.Length; i++)
        {
            Vector3 dir = Vector3.zero;
            Color color = chars[i].color;

            if (chars[i].shakeRadius > 0.0f)
            {
                float rad = UnityEngine.Random.Range(0, 360) * Mathf.Deg2Rad;
                dir += new Vector3(chars[i].shakeRadius * Mathf.Cos(rad), chars[i].shakeRadius * Mathf.Sin(rad), 0);
            }
            if (chars[i].waveDist > 0.0f)
            {
                dir += new Vector3(0, chars[i].waveDist * Mathf.Sin(i + Time.time * WaveSpeed), 0);
            }
            if (chars[i].circleRadius > 0.0f)
            {
                dir += new Vector3(chars[i].circleRadius * Mathf.Cos(i + Time.time * CircleSpeed), chars[i].circleRadius * Mathf.Sin(i + Time.time * CircleSpeed), 0);
            }
            if (chars[i].colorful)
            {
                color = new Color(UnityEngine.Random.value, UnityEngine.Random.value, UnityEngine.Random.value);
            }
            if (chars[i].scale != Vector3.one)
            {
                var center = vertices[i * 6 + 0].position + vertices[i * 6 + 1].position + vertices[i * 6 + 2].position + vertices[i * 6 + 4].position;
                center /= 4.0f;
                for (int c = 0; c < 6; c++)
                {
                    var vert = vertices[c + i * 6];
                    vert.position.x = Mathf.LerpUnclamped(center.x, vert.position.x, chars[i].scale.x);
                    vert.position.y = Mathf.LerpUnclamped(center.y, vert.position.y, chars[i].scale.y);
                    vertices[c + i * 6] = vert;
                }
            }
            if (chars[i].rot != 0.0f)
            {
                var center = vertices[i * 6 + 0].position + vertices[i * 6 + 1].position + vertices[i * 6 + 2].position + vertices[i * 6 + 4].position;
                center /= 4.0f;
                for (int c = 0; c < 6; c++)
                {
                    var vert = vertices[c + i * 6];
                    vert.position -= center;
                    vert.position = Quaternion.FromToRotation(Vector3.up, new Vector3(Mathf.Sin(-0.25f * i + Time.time * chars[i].rot), Mathf.Cos(-0.25f * i + Time.time * chars[i].rot))) * vert.position;
                    vert.position += center;
                    vertices[c + i * 6] = vert;
                }
            }
            if (chars[i].random != 0.0f)
            {
                var rands = new Vector3[6];
                var r = chars[i].random;
                for (var c = 0; c < 6; c++)
                {
                    var rad = UnityEngine.Random.Range(0, 360) * Mathf.Deg2Rad;
                    rands[c] = new Vector3(r * Mathf.Cos(rad), r * Mathf.Sin(rad));
                }
                rands[3] = rands[2];
                rands[5] = rands[0];
                for (int c = 0; c < 6; c++)
                {
                    var vert = vertices[c + i * 6];
                    vert.position += rands[c];
                    vertices[c + i * 6] = vert;
                }
            }
            if (chars[i].randscale != 0.0f)
            {
                var center = vertices[i * 6 + 0].position + vertices[i * 6 + 1].position + vertices[i * 6 + 2].position + vertices[i * 6 + 4].position;
                center /= 4.0f;
                var r = (UnityEngine.Random.value * 2.0f - 1.0f) * chars[i].randscale;
                for (int c = 0; c < 6; c++)
                {
                    var vert = vertices[c + i * 6];
                    var d = (vert.position - center).normalized * r;
                    vert.position += d;
                    vertices[c + i * 6] = vert;
                }
            }
            if (chars[i].small != 0.0f)
            {
                var center = vertices[i * 6 + 0].position + vertices[i * 6 + 1].position + vertices[i * 6 + 2].position + vertices[i * 6 + 4].position;
                center /= 4.0f;
                var s = (chars[i].small + 1.0f + i * Interval) - totaltime;
                for (int c = 0; c < 6; c++)
                {
                    var vert = vertices[c + i * 6];
                    vert.position = Vector3.Lerp(center, vert.position, s);
                    vertices[c + i * 6] = vert;
                }
            }
            if (chars[i].fadeout != 0.0f)
            {
                var center = vertices[i * 6 + 0].position + vertices[i * 6 + 1].position + vertices[i * 6 + 2].position + vertices[i * 6 + 4].position;
                center /= 4.0f;
                var a = (chars[i].fadeout + 1.0f + i * Interval) - totaltime;
                for (int c = 0; c < 6; c++)
                {
                    var vert = vertices[c + i * 6];
                    color.a = Mathf.Clamp01(a);
                    vertices[c + i * 6] = vert;
                }
            }
            if (chars[i].fadein)
            {
                var center = vertices[i * 6 + 0].position + vertices[i * 6 + 1].position + vertices[i * 6 + 2].position + vertices[i * 6 + 4].position;
                center /= 4.0f;
                var a = totaltime - (i * Interval);
                if (a <= 1.0f)
                {
                    for (int c = 0; c < 6; c++)
                    {
                        var vert = vertices[c + i * 6];
                        color.a = Mathf.Clamp01(a);
                        vertices[c + i * 6] = vert;
                    }
                }
            }
            if (chars[i].flash > 0.0f)
            {
                var flash = (Time.time % chars[i].flash * 2.0f) > chars[i].flash ? true : false;
                for (int c = 0; c < 6; c++)
                {
                    var vert = vertices[c + i * 6];
                    color.a = flash ? (byte)255 : (byte)0;
                    vertices[c + i * 6] = vert;
                }
            }

            for (int c = 0; c < 6; c++)
            {
                var vert = vertices[c + i * 6];
                vert.color = color;
                vert.position = vert.position + dir;
                vertices[c + i * 6] = vert;
            }
        }
    }

    void TextAlpha(ref List<UIVertex> vertices)
    {
        if (isEnd) return;

        var output = new List<UIVertex>();
        var vertexTop = charCount * 6;

        if (vertexTop >= vertices.Count)
        {
            isEnd = true;

            return;
        }

        for (int i = 0; i < vertexTop; ++i)
        {
            output.Add(vertices[i]);
        }

        for (int i = vertexTop; i < vertexTop + 6; ++i)
        {
            var uiVertex = vertices[i];
            if (uiVertex.color.a == 255) uiVertex.color.a = (byte)(255f * alpha);
            output.Add(uiVertex);
        }

        alpha += TextSpeed * Interval;
        while (alpha >= 1.0f)
        {
            charCount++;
            alpha -= 1.0f;
        }

        vertices = output;
    }

    void Update()
    {
        if (!Application.isPlaying) return;

        srcText = text.text;
        time += Time.deltaTime;
        totaltime += Time.deltaTime;
        if (time >= Interval)
        {
            time -= Interval;
            g.SetVerticesDirty();
        }
    }

    override protected void OnDestroy()
    {
        Init();
    }
}
