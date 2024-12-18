using System.Collections.Generic;
using UnityEngine;

[System.Serializable] public struct Box
{
    public GameObject go;

    [Range(1, 100)] public float weight;

    public int hash00R, hash00F, hash00L, hash00B;//Hash of colors of side
    public int hash10R, hash10F, hash10L, hash10B;//Can't have single value per side, we have to check directionally
}

[System.Serializable] public struct gBox
{
    public int x, y;
    public Box box;
    public int question;
    public float gweight;


    public float entropy;
    public float accuracy;


    public byte[] posb;//possibility indices
}

public class WFCBoom : MonoBehaviour
{
    [SerializeField] bool stepPropogate = true;    

    public Giraffe uigraph;
    public GameObject uitext;

    public float VoxelSize = 0.1f;
    public int numVoxel = 8;

    public int gX = 10, gY = 10;

    public Box[] arr_Boxes;

    public gBox[] arr_gBoxes;

    S_Input_Controls S_Input;

    private void Awake()
    {
        S_Input = new S_Input_Controls();
    }

    private void OnEnable()
    {
        S_Input.Enable();
    }

    private void OnDisable()
    {
        S_Input.Disable();
    }

    public void CalculateSidesColors(ref Box b)
    {
        byte[] colorsR = new byte[numVoxel * numVoxel];
        byte[] colorsF = new byte[numVoxel * numVoxel];
        byte[] colorsL = new byte[numVoxel * numVoxel];
        byte[] colorsB = new byte[numVoxel * numVoxel];

        for (int y = 0; y < numVoxel; y++)
        {
            for (int x = 0; x < numVoxel; x = x + 4)
            {
                int l = y * numVoxel + x;

                colorsR[l] = GetVoxelColor(b.go, x, y, Vector3.right);
                colorsF[l] = GetVoxelColor(b.go, x, y, Vector3.forward);
                colorsL[l] = GetVoxelColor(b.go, x, y, Vector3.left);
                colorsB[l] = GetVoxelColor(b.go, x, y, Vector3.back);

                colorsR[l + 1] = GetVoxelColor(b.go, x + 1, y, Vector3.right);
                colorsF[l + 1] = GetVoxelColor(b.go, x + 1, y, Vector3.forward);
                colorsL[l + 1] = GetVoxelColor(b.go, x + 1, y, Vector3.left);
                colorsB[l + 1] = GetVoxelColor(b.go, x + 1, y, Vector3.back);

                colorsR[l + 2] = GetVoxelColor(b.go, x + 2, y, Vector3.right);
                colorsF[l + 2] = GetVoxelColor(b.go, x + 2, y, Vector3.forward);
                colorsL[l + 2] = GetVoxelColor(b.go, x + 2, y, Vector3.left);
                colorsB[l + 2] = GetVoxelColor(b.go, x + 2, y, Vector3.back);

                colorsR[l + 3] = GetVoxelColor(b.go, x + 3, y, Vector3.right);
                colorsF[l + 3] = GetVoxelColor(b.go, x + 3, y, Vector3.forward);
                colorsL[l + 3] = GetVoxelColor(b.go, x + 3, y, Vector3.left);
                colorsB[l + 3] = GetVoxelColor(b.go, x + 3, y, Vector3.back);
            }
        }

        for (int y = 0; y < numVoxel; y++)
        {
            for (int x = 0; x < numVoxel; x = x + 4)
            {
                int l = y * numVoxel + x;

                b.hash00R = b.hash00R -
                            ((int)colorsR[l] << 24 | (int)colorsR[l + 1] << 16
                            | (int)colorsR[l + 2] << 8 | (int)colorsR[l + 3] << 0);
                b.hash00B = b.hash00B -
                            ((int)colorsB[l] << 24 | (int)colorsB[l + 1] << 16
                            | (int)colorsB[l + 2] << 8 | (int)colorsB[l + 3] << 0);
                b.hash00L = b.hash00L -
                            ((int)colorsL[l] << 24 | (int)colorsL[l + 1] << 16
                            | (int)colorsL[l + 2] << 8 | (int)colorsL[l + 3] << 0);
                b.hash00F = b.hash00F -
                            ((int)colorsF[l] << 24 | (int)colorsF[l + 1] << 16
                            | (int)colorsF[l + 2] << 8 | (int)colorsF[l + 3] << 0);
            }
        }

        for (int y = 0; y < numVoxel; y++)
        {
            for (int x = numVoxel - 1; x >= 0; x = x - 4)
            {
                int l = y * numVoxel + x;

                b.hash10R = b.hash10R -
                            ((int)colorsR[l] << 24 | (int)colorsR[l - 1] << 16
                            | (int)colorsR[l - 2] << 8 | (int)colorsR[l - 3] << 0);
                b.hash10B = b.hash10B -
                            ((int)colorsB[l] << 24 | (int)colorsB[l - 1] << 16
                            | (int)colorsB[l - 2] << 8 | (int)colorsB[l - 3] << 0);
                b.hash10L = b.hash10L -
                            ((int)colorsL[l] << 24 | (int)colorsL[l - 1] << 16
                            | (int)colorsL[l - 2] << 8 | (int)colorsL[l - 3] << 0);
                b.hash10F = b.hash10F -
                            ((int)colorsF[l] << 24 | (int)colorsF[l - 1] << 16
                            | (int)colorsF[l - 2] << 8 | (int)colorsF[l - 3] << 0);
            }
        }
    }

    public Box R90(Box b)
    {
        Box o = new Box();

        GameObject clone = Instantiate(b.go, b.go.transform.position + 2 * Vector3.right, b.go.transform.rotation, b.go.transform.parent);
        clone.transform.Rotate(0, -90, 0);
        clone.name = b.go.name + " X";
        o.go = clone;

        o.weight = b.weight;

        o.hash00R = b.hash00B;
        o.hash00F = b.hash00R;
        o.hash00L = b.hash00F;
        o.hash00B = b.hash00L;

        o.hash10R = b.hash10B;
        o.hash10F = b.hash10R;
        o.hash10L = b.hash10F;
        o.hash10B = b.hash10L;

        return o;
    }

    private byte GetVoxelColor(GameObject go, int x, int y, Vector3 rayDir)
    {
        var mc = go.GetComponentInChildren<MeshCollider>();

        float vox = VoxelSize;
        float t = 8 * vox;
        float half = VoxelSize / 2;

        Vector3 rayStart = Vector3.zero;

        Color debugCol = Color.white;

        if (rayDir == Vector3.right)
        {
            rayStart = mc.bounds.min + new Vector3(t + half, half + y * vox, half + x * vox);
            debugCol = Color.red;
        }
        else if (rayDir == Vector3.forward)
        {
            rayStart = mc.bounds.min + new Vector3(t - half - x * vox, half + y * vox, t + half);
            debugCol = Color.blue;
        }

        else if (rayDir == Vector3.left)
        {
            rayStart = mc.bounds.min + new Vector3(-half, half + y * vox, t - half - x * vox);
            debugCol = Color.green;
        }

        else if (rayDir == Vector3.back)
        {
            rayStart = mc.bounds.min + new Vector3(half + x * vox, half + y * vox, -half);
            debugCol = Color.yellow;
        }

//        Debug.DrawRay(rayStart, rayDir * 0.05f, debugCol, 200);

        if (Physics.Raycast(new Ray(rayStart, -rayDir * vox), out RaycastHit hit, vox))
        {
            byte colorIndex = (byte)(hit.textureCoord.x * 256);

            return colorIndex;
        }

        return 0;
    }

    private void collapse(ref gBox g)
    {
        float value = Random.Range(0, g.gweight);
        
        float sum = 0;

        for(int i = 0, l = arr_Boxes.Length; i < l; i++)
        {
            int d = i / 8;
            if(bitAt(g.posb[d], (byte)(i - d * 8)) == 1)
            {
                g.box = arr_Boxes[i];
    
                sum += arr_Boxes[i].weight;
                if(sum >= value)
                {
                    g.box.go = Instantiate(g.box.go, new Vector3(g.x, 0, g.y), g.box.go.transform.rotation, transform);
                    g.question = 0;
                    g.gweight = 0;
                    for(int i0 = 0; i0 < (l/8 + 1); i0++)
                    {
                        g.posb[i0] = 0;
                    }
                    gBoxAccuracy(ref g);
                    gBoxEntropy(ref g);

                    if(g.y < (gY - 1))
                    {
                        ref gBox gt = ref arr_gBoxes[(g.y + 1) * gX + g.x];

                        for(int n = 0; n < l; n++)
                        {
                            int m = n / 8;

                            if((bitAt(gt.posb[m], (byte)(n - m * 8)) == 1) && (arr_Boxes[n].hash00B != g.box.hash10F))
                            {
                                setBitAt(ref gt.posb[m], (byte)(n - m * 8), 0);
                                gt.question -= 1;
                                gt.gweight -= arr_Boxes[n].weight;                                
                            }
                        }
                        gBoxAccuracy(ref gt);
                        gBoxEntropy(ref gt);

                        //GetRandomTile(ref gt);
                    }
                    if(g.y > 0)
                    {
                        ref gBox gb = ref arr_gBoxes[(g.y - 1) * gX + g.x];

                        for(int n = 0; n < l; n++)
                        {
                            int m = n / 8;

                            if((bitAt(gb.posb[m], (byte)(n - m * 8)) == 1) && (arr_Boxes[n].hash00F != g.box.hash10B))
                            {
                                setBitAt(ref gb.posb[m], (byte)(n - m * 8), 0);
                                gb.question -= 1;
                                gb.gweight -= arr_Boxes[n].weight;
                            }
                        }
                        gBoxAccuracy(ref gb);
                        gBoxEntropy(ref gb);

                        //GetRandomTile(ref gb);
                    }
                    if(g.x < (gX - 1))
                    {
                        ref gBox gr = ref arr_gBoxes[g.y * gX + g.x + 1];

                        for(int n = 0; n < l; n++)
                        {
                            int m = n / 8;

                            if((bitAt(gr.posb[m], (byte)(n - m * 8)) == 1) && (arr_Boxes[n].hash00L != g.box.hash10R))
                            {
                                setBitAt(ref gr.posb[m], (byte)(n - m * 8), 0);
                                gr.question -= 1;
                                gr.gweight -= arr_Boxes[n].weight;
                            }
                        }
                        gBoxAccuracy(ref gr);
                        gBoxEntropy(ref gr);

                        //GetRandomTile(ref gr);
                    }
                    if(g.x > 0)
                    {
                        ref gBox gl = ref arr_gBoxes[g.y * gX + g.x - 1];

                        for(int n = 0; n < l; n++)
                        {
                            int m = n / 8;

                            if((bitAt(gl.posb[m], (byte)(n - m * 8)) == 1) && (arr_Boxes[n].hash00R != g.box.hash10L))
                            {
                                setBitAt(ref gl.posb[m], (byte)(n - m * 8), 0);
                                gl.question -= 1;
                                gl.gweight -= arr_Boxes[n].weight;
                            }
                        }
                        gBoxAccuracy(ref gl);
                        gBoxEntropy(ref gl);

                        //GetRandomTile(ref gl);
                    }

                    return;
                }
            }
        }
    }

    void Start()
    {
        for (int i = 0, l = arr_Boxes.Length; i < l; i++)
        {
            CalculateSidesColors(ref arr_Boxes[i]);
        }

        List<Box> list_Boxes = new List<Box>();

        for (int i = 0, l = arr_Boxes.Length; i < l; i++)
        {
            if ((arr_Boxes[i].hash00L ^ arr_Boxes[i].hash00B ^ arr_Boxes[i].hash00F ^ arr_Boxes[i].hash00R) == 0)
            {
                list_Boxes.Add(arr_Boxes[i]);
                continue;
            }

            if ((arr_Boxes[i].hash00L == arr_Boxes[i].hash00R)
                && (arr_Boxes[i].hash00F == arr_Boxes[i].hash00B)
                && (arr_Boxes[i].hash00F != arr_Boxes[i].hash00R))
            {
                arr_Boxes[i].weight /= 2;
                list_Boxes.Add(arr_Boxes[i]);
                list_Boxes.Add(R90(arr_Boxes[i]));
                continue;
            }

            arr_Boxes[i].weight /= 4;
            list_Boxes.Add(arr_Boxes[i]);
            Box b90 = R90(arr_Boxes[i]); list_Boxes.Add(b90);
            Box b180 = R90(b90); list_Boxes.Add(b180);
            Box b270 = R90(b180); list_Boxes.Add(b270);
        }

        arr_Boxes = list_Boxes.ToArray();

        newGrid();

        StartCoroutine(WaitForKeyDown());
    }

    System.Collections.IEnumerator WaitForKeyDown()
    {
        int s = 0;
        while (!S_Input.MiscKeyMap.Escape.triggered)
        {
            if(S_Input.MiscKeyMap.Space.triggered){

                if(s == arr_gBoxes.Length) 
                {
                    s = 0;
                    destroyGrid();
                }
                
                if(stepPropogate)
                {
                    propogate();
                } else {
                    destroyGrid();
                
                    for (int m = 0, l = arr_gBoxes.Length; m < l; m++)
                    {
                        propogate();
                    }
                }

                s++;
            }
            yield return null;
        }
        yield return new WaitForSeconds(1f);
        Debug.Log("Done");
    }

    void propogate()
    {
        int oo = 0;

        for (int m = 0, l = arr_gBoxes.Length; m < l; m++)
        {
            if(arr_gBoxes[m].question > 0)
            {
                oo = m;
                break;
            }
        }

        for (int m = oo+1, l = arr_gBoxes.Length; m < l; m++)
        {
            int ooq = arr_gBoxes[oo].question;
            int mq = arr_gBoxes[m].question;

            if( (mq > 0) && 
                ((mq < ooq) || 
                ((mq == ooq) && (arr_gBoxes[m].gweight > arr_gBoxes[oo].gweight))))
            {
                oo = m;
            }
        }

        collapse(ref arr_gBoxes[oo]);
        uitext.GetComponent<TMPro.TextMeshProUGUI>().text = "(" + arr_gBoxes[oo].x + "," + arr_gBoxes[oo].y + ")  e: " + 
                    arr_gBoxes[oo].question + "  w" + arr_gBoxes[oo].gweight + "  p0  " + arr_gBoxes[oo].posb[0] + "  p1  " + arr_gBoxes[oo].posb[1];

    }

    void newGrid()
    {
        arr_gBoxes = new gBox[gX * gY];
        
        for (int y = 0; y < gY; y++)
        {
            for (int x = 0; x < gX; x++)
            {
                ref gBox b = ref arr_gBoxes[y * gX + x];

                b.x = x;
                b.y = y;
                b.posb = new byte[(arr_Boxes.Length/8 + 1)];

                int question = 0;
                float weight = 0;
                for(int i = 0, l = arr_Boxes.Length; i < l; i++)
                {
                    int d = i / 8;
                    setBitAt(ref b.posb[d], (byte)(i - d * 8), 1);

                    question += 1;
                    weight += arr_Boxes[i].weight;
                }
                b.question = question;
                b.gweight = weight;
                gBoxAccuracy(ref b);
                gBoxEntropy(ref b);
            }
        }
    }

    void destroyGrid()
    {
        for (int m = 0, l = arr_gBoxes.Length; m < l; m++)
        {
            Destroy(arr_gBoxes[m].box.go);
        }
        newGrid();
    }

    void gBoxEntropy(ref gBox g)
    {
        float sumofLogweights = 0;

        for(int i = 0,  l = arr_Boxes.Length; i < l; i++)
        {
            int d = i / 8;
            if(bitAt(g.posb[d], (byte)(i - d * 8)) == 1)
            {
                float prob = arr_Boxes[i].weight / g.gweight;
                sumofLogweights += prob * Mathf.Log(prob);
            }
        }

        g.entropy = - sumofLogweights;
    }

    void gBoxAccuracy(ref gBox g)
    {
        float productofpowers = 1;

        for(int i = 0,  l = arr_Boxes.Length; i < l; i++)
        {
            int d = i / 8;
            if(bitAt(g.posb[d], (byte)(i - d * 8)) == 1)
            {
                float prob = arr_Boxes[i].weight / g.gweight;
                productofpowers *= Mathf.Pow(prob, prob);
            }
        }

        g.accuracy = productofpowers;
    }

    byte bitAt(byte x, byte i)
    {
        return (byte)(x >> i & 1);
    }

    void setBitAt(ref byte x, byte i, byte v)
    {
        byte f1 = (byte)(1 << i);
        byte f0 = (byte)(255 - f1);
        byte vShift = (byte)(v << i);
        x = (byte)((x & f0) | vShift);
    }
}
