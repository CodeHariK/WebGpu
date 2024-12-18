using System.Collections.Generic;
using UnityEngine;

public static class Hat
{
	public static float Dot2(Vector2 a, Vector2 b)
	{
		return a.x * b.x + a.y + b.y;
	}

	public static float Cross2(Vector2 a, Vector2 b)
	{
		return a.x * b.y - a.y * b.x;
	}

	//Return x,y coords and sqrRadius
	public static Vector3 Circum(Vector2 a, Vector2 b, Vector2 c)
	{
		Vector2 ca = c - a;
		Vector2 ba = b - a;

		float cal = ca.sqrMagnitude;
		float bal = ba.sqrMagnitude;

		float d = 0.5f / Cross2(ba, ca);

		float dx = (ca.y * bal - ba.y * cal) * d;
		float dy = (ba.x * cal - ca.x * bal) * d;

		return new Vector3(a.x + dx, a.y + dy, dx * dx + dy * dy);
	}

	//Generics are Super Slow, Comparaters are bad, Inheritance is bad, Dynamic code is bad, Convert list to array(free), All abstractions are bad.
	public static void MVecXSort(Vector2[] h)
	{
		Vector2[] r = new Vector2[h.Length];

		void Local_Merge(int l, int m, int n)//l = starting index, m = len of first arr, n = len of 2 arr
		{
			int u = l + m;

			for(int a = 0, b = 0; (a + b) < (m + n); )
			{
				if(a == m)
				{
					r[l + a + b] = h[u + b];
					b++;
					continue;
				}
				if(b == n)
				{
					r[l + a + b] = h[l + a];
					a++;
					continue;
				}

				if(h[u + b].x < h[l + a].x)
				{
					r[l + a + b] = h[u + b];
					b++;
				} else 
				{
					r[l + a + b] = h[l + a];
					a++;
				}
			}

			for(int a = 0; a < (m + n); a++)
			{
				h[l + a] = r[l + a];
			}
		}

		void Local_Divide(int l, int u)//l = starting index, u = last index
		{
			int m = (l+u)/2;

			if((u - m) >= 1)
			{
				Local_Divide(l, m);
				Local_Divide(m+1, u);
			}
			Local_Merge(l, m - l + 1, u - m);
		}

		Local_Divide(0, h.Length-1);
	}

	public static int binSearch(float[] arr, float a)
	{
		int l = arr.Length;
		int lb = 0;
		int ub = l - 1;
		int x = l/2;
		
		if(a == arr[0]) return 0;
		if(a < arr[lb]) return -1;
		if(a > arr[ub]) return l+1;

		for(; l > 1; l = l >> 1)
		{
			if(a <= arr[x - 1])
			{
				ub = x - 1;
			}
			else {
				lb = x;
			}
			x = (ub+lb)/2;
		}

		if((arr[ub] - a) < (a - arr[lb])) return (x + 1);

		return x;
	}

	public static Vector2 quadBezier(Vector2 c0,  Vector2 m, Vector2 c1, float t)
	{
		float n = (1 - t);

		float tt = t * t;
		float tn = 2 * t * n;
		float nn = n * n;

		return (nn * c0 + tn * m + tt * c1);
	}

	public static Vector2 cubicBezier(Vector2 c0, Vector2 m0, Vector2 m1, Vector2 c1, float t)
	{
		float n = (1 - t);

		float tt = t * t;
		float nn = n * n;

		float ttt = tt * t;
		float ttn = 3 * tt * n;
		float nnt = 3 * nn * t;
		float nnn = nn * n;

		return (nnn * c0 + nnt * m0 + ttn * m1 + ttt * c1);
	}

	/*return coord of perpDis of line at XonLine Dis from absX Dis*/
	public static Vector2 eqLine(Vector2 abs, float dison/*Dis on Line*/, float pdis/*Perp Dis*/, Vector2 c0, Vector2 c1)
	{
		float relX = abs.x - c0.x;

		Vector2 sc = sinCos(c0, c1);
		float 	m  = (c0.x != c1.x) ? sc.x / sc.y : 0;

		if(c0.x == c1.x) return new Vector2(c0.x + pdis, abs.y);

		//float perpM		= (c1.y != c0.y) ? -(c1 - c0).x / (c1 - c0).y : 0;

		return new Vector2( c0.x + relX 	+ dison * sc.y - pdis * sc.x,
							c0.y + relX * m + dison * sc.x + pdis * sc.y);
	}

	public static Vector2 lineIntersection(Vector2 c0, Vector2 sc0, Vector2 c1, Vector2 sc1)
	{
		Vector2 I = Vector2.zero;

		if(sc0.y == 0 || sc1.y == 0) Debug.Log("I am Loki");

		float m0 = sc0.x / sc0.y;
		float m1 = sc1.x / sc1.y;

		if(Mathf.Approximately(m0, m1)) return ((c0 + c1)/2);

		if(Mathf.Abs(m0) == Mathf.Infinity) return eqLine(c0, 0, 0, c1, c1 + new Vector2(1, m1));
		if(Mathf.Abs(m1) == Mathf.Infinity) return eqLine(c1, 0, 0, c0, c0 + new Vector2(1, m0));

		I.x = (c0.y - c1.y - m0 * c0.x + m1 * c1.x)/(m1 - m0);
		I.y = c0.y + m0 * (I.x - c0.x);

		return I;
	}

	public static Vector2 sinCos(Vector2 c0, Vector2 c1)
	{
		float hyp = (c1 - c0).magnitude;
		float sin = (c1 - c0).y / hyp;
		float cos = (c1 - c0).x / hyp;

		return new Vector2(sin, cos);
	}

	public static Vector2 eqSmoothLine(float absX, int curve, Vector2 c0, Vector2 c1, Vector2 v0, Vector2 v1)
	{
		float a = eqLine(new Vector2(absX,0), 0, 0, c0, c1).y;
		float b = eqLine(new Vector2(absX,0), 0, 0, v0, v1).y;

		float k = curve * 20.0f;
		float h = Mathf.Max(0, Mathf.Min(1,(b - a)/k + 0.5f));
		float m = h * (1 - h) * k;
		
		return new Vector2(absX, h * a + (1 - h) * b - m * 0.5f);
	}

	public static Vector2[] smoothCurve(Vector2[] points, bool loop, float samplingT)
	{
		List<Vector2> smoo = new List<Vector2>(64);

		int l = points.Length;

		if(!loop) smoo.Add(points[0]);

		for(int i = loop ? 0 : 1, n = loop ? l : (l - 1); i < n; i++)
		{
			Vector2 k_n = points[(i-1 + l)%l];
			Vector2 k 	= points[i];
			Vector2 k_p = points[(i+1 + l)%l];

			Vector2 p0 = 0.1f * k_n + 0.9f * k;
			Vector2 p1 = 0.9f * k 	+ 0.1f * k_p;
			int s = (int)(1/samplingT);

			Vector2 sc0 = sinCos(k_n, p0);
			Vector2 sc1 = sinCos(k, p1);
			Vector2 I 	= lineIntersection(p0, sc0, p1, sc1);

			/*s = (int)((p1 - p0).x/samplingT);
			int curve = 1;
			if (eqLine(p0, 0, 0, points[i-1], points[i]).y > eqLine(p0, 0, 0, points[i], points[i + 1]).y)
				curve = -1;*/

			for(int o = 0; o <= s; o++)
			{
				smoo.Add(quadBezier(p0, I, p1, o * samplingT));
				//smoo.Add(cubicBezier(p0, 0.8f * I + 0.2f * p0, 0.8f * I + 0.2f * p1, p1, o * samplingT));
				/*
				float X = p0.x + (o * samplingT);
				smoo.Add(
					eqSmoothLine(X, curve,
									points[i - 1], points[i], 
									points[i], points[i + 1])
				);*/
			}
		}
	
		if(!loop) smoo.Add(points[l - 1]);

		Vector2[] arr_smoo = smoo.ToArray();

		return arr_smoo;
	}

	public static Vector2[] circlePoints(float r, int secIn360, float portionAngle, float iniAngle)
	{
		int s = (int)((portionAngle / 360) * secIn360);
		float theta = Mathf.Deg2Rad * (portionAngle/s);
		iniAngle = Mathf.Deg2Rad * iniAngle;

		if(portionAngle%360 != 0) s++;

		Vector2[] points = new Vector2[s];

		for(int i = 0; i < s; i++)
		{
			points[i] = new Vector2(r * Mathf.Cos(iniAngle + i * theta), r * Mathf.Sin(iniAngle + i * theta));
		}

		return points;
	}

	public static List<Vector2> GramHull(Vector2[] points)
	{
		MVecXSort(points);

		Vector2 lef = points[0];
		Vector2 rl = points[points.Length - 1] - lef;

		List<Vector2> up = new List<Vector2>(10){lef};
		List<Vector2> dn = new List<Vector2>(10){lef};

		float uAng = 0, dAng = 0;

		for(int i = 1, l = points.Length; i < l; i++)
		{
			Vector2 p = points[i];
			
			float c = Cross2(rl, p - lef);

			if(c >= 0)
			{
				float ang = (p - up[up.Count - 1]).y / (p - up[up.Count - 1]).x;
				for(bool run = true; (up.Count > 1) && run;)
				{
					if(ang > uAng)
					{
						up.RemoveAt((up.Count - 1));

						if(up.Count > 1)
							uAng = (up[up.Count - 1] - up[up.Count - 2]).y / (up[up.Count - 1] - up[up.Count - 2]).x;
						ang = (p - up[up.Count - 1]).y / (p - up[up.Count - 1]).x;
					}
					else {run = false;}
				}
				up.Add(p);
				uAng = ang;
			}

			if(c <= 0)
			{
				float ang = (p - dn[dn.Count - 1]).y / (p - dn[dn.Count - 1]).x;
				for(bool run = true; (dn.Count > 1) && run;)
				{
					if(ang < dAng)
					{
						dn.RemoveAt(dn.Count - 1);

						if(dn.Count > 1)
							dAng = (dn[dn.Count - 1] - dn[dn.Count - 2]).y / (dn[dn.Count - 1] - dn[dn.Count - 2]).x;
						ang = (p - dn[dn.Count - 1]).y / (p - dn[dn.Count - 1]).x;
					}
					else {run = false;}
				}
				dn.Add(p);
				dAng = ang;
			}
		}
		dn.RemoveAt(0);
		dn.RemoveAt(dn.Count - 1);
		dn.Reverse();
		up.AddRange(dn);
		return up;
	}

	public static Vector2[] ChanHull(Vector2[] points)
	{
		List<Vector2> p = new List<Vector2>(128);
		p.AddRange(points);

		for(int l = p.Count, d = 128; d<l; d *= 2)
		{
			List<Vector2> s = new List<Vector2>(32);
			int n = (l/d);
			for(int i = 0; i < n; i++)
			{
				List<Vector2> t = new List<Vector2>();
				for(int x = 0; x <  ((i != n - 1) ? d : d + (l - n * d)); x++)
				{
					t.Add(p[i * d + x]);
				}
				s.AddRange(GramHull(t.ToArray()));
			}

			p = s;
            //Debug.Log("  ->   d : " + d + "      le " + l + "  -> " + p.Count);
			l = p.Count;
		}

		Vector2[] hull = GramHull(p.ToArray()).ToArray();

		return hull;
	}

	public static Vector2[] DoomHull(Vector2[] points)
	{
		List<Vector2> sGram(Vector2[] g, bool upside)
		{
			MVecXSort(g);
			List<Vector2> q = new List<Vector2>(64){g[0]};
			float udAng = 0;
			for(int i = 1, l = g.Length; i < l; i++)
			{
				Vector2 p = g[i];			
				float ang = (p - q[q.Count - 1]).y / (p - q[q.Count - 1]).x;
				for(bool run = true; (q.Count > 1) && run;)
				{
					if(!(upside ^ (ang > udAng)))
					{
						q.RemoveAt((q.Count - 1));
						if(q.Count > 1)
							udAng = (q[q.Count - 1] - q[q.Count - 2]).y / (q[q.Count - 1] - q[q.Count - 2]).x;
						ang = (p - q[q.Count - 1]).y / (p - q[q.Count - 1]).x;
					}
					else {run = false;}
				}
				q.Add(p);
				udAng = ang;
			}
			return q;
		}

		List<Vector2> Chan(Vector2[] p, bool upside)
		{
			Vector2[] t;
			for(int l = p.Length, d = 128; d<l; d *= 2)
			{
				List<Vector2> s = new List<Vector2>(32);

				for(int i = 0, n = (l/d); i < n; i++)
				{
					int c = ((i != n - 1) ? d : d + (l - n * d));
					t = new Vector2[c];
					for(int x = 0; x < c; x++)
					{
						t[x] = p[i * d + x];
					}
					s.AddRange(sGram(t, upside));
				}

				p = s.ToArray();
				l = p.Length;
			}

			return sGram(p, upside);
		}

		Vector2 lef = points[0], rig = lef, top = lef, bot = lef;

		for(int v = 0, l = points.Length; v < l; v++)
		{
			Vector2 p = points[v];
			if(p.x < lef.x) lef = p;
			if(rig.x < p.x) rig = p;
			if(p.y < bot.y) bot = p;
			if(top.y < p.y) top = p;
		}

		Vector2 tr = top - rig, lt = lef - top, bl = bot - lef, rb = rig - bot;

		List<Vector2> ltL = new List<Vector2>(128){lef, top};
		List<Vector2> trL = new List<Vector2>(128){top, rig};
		List<Vector2> rbL = new List<Vector2>(128){bot, rig};
		List<Vector2> blL = new List<Vector2>(128){lef, bot};

		for(int v = 0, l = points.Length; v < l; v++)
		{
			Vector2 p = points[v];

			if(Cross2(lt, p - top) < 0) {ltL.Add(p); continue;}
			if(Cross2(tr, p - rig) < 0) {trL.Add(p); continue;}
			if(Cross2(rb, p - bot) < 0)	{rbL.Add(p); continue;}
			if(Cross2(bl, p - lef) < 0)	{blL.Add(p); continue;}
		}

		List<Vector2> up = Chan(ltL.ToArray(), true);
		up.RemoveAt(up.Count - 1);
		up.AddRange(Chan(trL.ToArray(), true));

		List<Vector2> dn = Chan(blL.ToArray(), false);
		dn.RemoveAt(dn.Count - 1);
		dn.AddRange(Chan(rbL.ToArray(), false));

		dn.RemoveAt(0);
		dn.RemoveAt(dn.Count - 1);
		dn.Reverse();

		up.AddRange(dn);
		return up.ToArray();
	}

	public static Vector2[,] SimplePoisson(float radius, Vector2 sampleRegionSize)
	{
		float cellSize = 2 * radius;

		int cellXCount = Mathf.CeilToInt(sampleRegionSize.x / cellSize);
		int cellYCount = Mathf.CeilToInt(sampleRegionSize.y / cellSize);

		Vector2[,] grid = new Vector2[cellXCount, cellYCount];

		for (int y = 0; y < cellYCount; y++)
		{
			for (int x = 0; x < cellXCount; x++)
			{
				grid[x, y] = new Vector2((x + 0.2f + Random.value * 0.6f) * cellSize, (y + 0.2f + Random.value * 0.6f) * cellSize);
			}
		}

		return grid;
	}

	public static Vector2[,] level2Poisson(float radius, Vector2 sampleRegionSize, int numSamplesBeforeRejection = 10, float cellSizeFactor = 0.7f)
	{
		float cellSize = 2 * radius * cellSizeFactor;
		float sqrDisAccepted = 1.2f * radius * radius;

		int cellXCount = Mathf.CeilToInt(sampleRegionSize.x / cellSize);
		int cellYCount = Mathf.CeilToInt(sampleRegionSize.y / cellSize);

		Vector2[,] grid = new Vector2[cellXCount, cellYCount];

		grid[0, 0] = new Vector2(cellSize * Random.value, cellSize * Random.value);
		Vector2 tryVec = Vector2.positiveInfinity;

		int x1 = 0, y1 = 0, xy = 0;

		for (int x = 1; x < cellXCount; x++)
		{
			tryVec = new Vector2((x + Random.value) * cellSize, cellSize * Random.value);
			grid[x, 0] = tryVec;

			for (int t = 0; t < numSamplesBeforeRejection; t++)
			{
				if (Vector2.SqrMagnitude(tryVec - grid[x - 1, 0]) > sqrDisAccepted)
				{
					grid[x, 0] = tryVec;
					x1++;
					break;
				}
				tryVec = new Vector2((x + Random.value) * cellSize, cellSize * Random.value);
			}
		}

		for (int y = 1; y < cellYCount; y++)
		{
			tryVec = new Vector2(cellSize * Random.value, (y + Random.value) * cellSize);
			grid[0, y] = tryVec;

			for (int t = 0; t < numSamplesBeforeRejection; t++)
			{
				if (Vector2.SqrMagnitude(tryVec - grid[0, y - 1]) > sqrDisAccepted)
				{
					grid[0, y] = tryVec;
					y1++;
					break;
				}
				tryVec = new Vector2(cellSize * Random.value, (y + Random.value) * cellSize);
			}
		}

		for (int y = 1; y < cellYCount; y++)
		{
			for (int x = 1; x < cellXCount; x++)
			{
				tryVec = new Vector2((x + Random.value) * cellSize, (y + Random.value) * cellSize);
				grid[x, y] = new Vector2((x + 0.5f) * cellSize, (y + 0.5f) * cellSize);

				xy++;

				for (int t = 0; t < numSamplesBeforeRejection; t++)
				{
					if (Vector2.SqrMagnitude(tryVec - grid[x - 1, y - 1]) > sqrDisAccepted
						&& Vector2.SqrMagnitude(tryVec - grid[x, y - 1]) > sqrDisAccepted
						&& Vector2.SqrMagnitude(tryVec - grid[x - 1, y]) > sqrDisAccepted
						//&& tryVec.y > (grid[x, y - 1].y + radius)
						//&& tryVec.x > (grid[x - 1, y].x + radius)
						)
					{
						grid[x, y] = tryVec;
						break;
					}
					tryVec = new Vector2((x + Random.value) * cellSize, (y + Random.value) * cellSize);
				}
			}
		}

		Debug.Log("x1 = " + x1 + " y1 = " + y1 + " xy = " + xy + " Xcount = " + cellXCount + " Ycount = " + cellYCount + " c = " + (x1 + y1 + xy));

		return grid;
	}

}
