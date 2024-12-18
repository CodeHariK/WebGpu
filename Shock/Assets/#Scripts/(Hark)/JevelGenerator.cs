//#define _Debug

using System.Collections.Generic;
using UnityEngine;

public struct contra
{
    public byte config;
    public byte Ry;

    public int probability;//--------------------------

    public contra(byte config, byte Ry, int probability)
    {
        this.config = config;
        this.Ry = Ry;
        this.probability = 0;
    }
}

[System.Serializable] public struct CornerBox
{
    public GameObject gameObject;
    public byte corners;
}

public class JevelGenerator : MonoBehaviour
{
    [Header("Corner Dimensions")]//Atleast 8 corner 
    [Range(2,5)]public int dimX = 3; 
    [Range(2,5)]public int dimY = 5;
    [Range(2,5)]public int dimZ = 3;
    [Space(10)]

    public GameObject MeshComplete;
    [Space(10)] public Dictionary<string, Mesh> iMeshes = new Dictionary<string, Mesh>(100);

    public CornerBox pf_cBox;
    public CornerBox[] arr_cBoxes;

    public GameObject activeGridBox;
    public GameObject pf_dummyGridBox;
    public GameObject[] arr_dummyGridBoxes;

    byte[] iC = {16,1,239,254,144,9,136,132,72,130,111,246,119,
                123,183,125,176,11,25,38,145,152,84,69,88,133,148,73,146,41,79,
                244,230,217,110,103,171,186,167,122,107,182,109,214,15,204,216,
                141,156,147,150,212,77,209,29,210,45,172,92,202,197,
                
                80,5,175,250,85,90};

    public contra[] arr_contra = new contra[256];

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

    void Start()
    {
        for(int i = 0; i < iC.Length; i++)
        {
            rAppendContra(arr_contra, iC[i], 4);
        }

        foreach (Transform child in MeshComplete.transform)
        {
            iMeshes.Add(child.name, child.GetComponent<MeshFilter>().sharedMesh);
        }

        arr_cBoxes = new CornerBox[dimX * dimY * dimZ];
        for(int x = 0; x < dimX - 1; x++)
        {
            for(int z = 0; z < dimZ - 1; z++)
            {
                enableGridBoxCorners(x, 0, z);
            }
        }

        arr_dummyGridBoxes = new GameObject[(dimX - 1) * (dimY - 1) * (dimZ - 1)];

		randomizeGridBox();

        fillCornerObjects();

        StartCoroutine(WaitForKeyDown());
    }

	void Update () {
		Ray ray = Camera.main.ScreenPointToRay(S_Input.MiscKeyMap.MouseLoc.ReadValue<Vector2>());
        RaycastHit hit;

		if(Physics.Raycast(ray, out hit) && hit.collider.tag == "Tag_GridBox")
		{
            if(activeGridBox != null)
                activeGridBox.GetComponent<MeshRenderer>().enabled = false;
            activeGridBox = hit.collider.gameObject;
			activeGridBox.GetComponent<MeshRenderer>().enabled = true;

            if(S_Input.MiscKeyMap.LMouseClick.triggered)
            {
                Vector3Int xyz = stringxyzExtract(activeGridBox.name);                    

                xyz.x += (int)hit.normal.x;
                xyz.y += (int)hit.normal.y;
                xyz.z += (int)hit.normal.z;

                if( xyz.x >= 0 && xyz.x < (dimX - 1) && 
                    xyz.y >= 0 && xyz.y < (dimY - 1) &&
                    xyz.z >= 0 && xyz.z < (dimZ - 1))
                {
                    enableGridBoxCorners(xyz.x, xyz.y, xyz.z, 1);
                    fillCornerObjects();

                    GameObject cInst = Instantiate(pf_dummyGridBox.gameObject, new Vector3(xyz.x + 0.5f, xyz.y + 0.5f, xyz.z + 0.5f), Quaternion.identity, this.transform);
                    arr_dummyGridBoxes[xyz.y * (dimX - 1) * (dimZ - 1) + xyz.z * (dimZ - 1) + xyz.x] = cInst;
                    cInst.name = "D_" + xyz.x + "x" + xyz.y + "y" + xyz.z + "z";
                }
            }

            if(S_Input.MiscKeyMap.RMouseClick.triggered)
            {
                Vector3Int xyz = stringxyzExtract(activeGridBox.name);

                enableGridBoxCorners(xyz.x, xyz.y, xyz.z, 0);
                fillCornerObjects();
                Destroy(activeGridBox);
            }
		}
	}

#region Builder

    void fillCornerObjects()
    {
        for (int y = 0; y < dimY; y++)
        {
            for (int z = 0; z < dimZ; z++)
            {
                for (int x = 0; x < dimX; x++)
                {
                    ref CornerBox b = ref arr_cBoxes[y * dimX * dimZ + z * dimZ + x];

                    arr_contra[b.corners].probability += 1; 

                    Destroy(b.gameObject);

                    GameObject cInst = Instantiate(pf_cBox.gameObject, new Vector3(x, y, z), Quaternion.identity, this.transform);
                    b.gameObject = cInst;

                    b.gameObject.name = "Gi_" + x + "_" + y + "_" + z + "_⚙_" + b.corners + "_c_" + arr_contra[b.corners].config + "_r_" + arr_contra[b.corners].Ry;

                    SetMesh(b, x, y, z);
                }
            }
        }
    }

    void enableGridBoxCorners(int x, int y, int z, byte b = 1)
	{
        int p = dimX * dimZ, q = dimX;

        int t = (y * p + z * q + x);

        setBitAt(ref arr_cBoxes[t          + p].corners, 1, b);//7
        setBitAt(ref arr_cBoxes[t + 1      + p].corners, 0, b);//6
        setBitAt(ref arr_cBoxes[t + 1 + q  + p].corners, 3, b);//5
        setBitAt(ref arr_cBoxes[t +     q  + p].corners, 2, b);//4
        setBitAt(ref arr_cBoxes[t             ].corners, 5, b);//3
        setBitAt(ref arr_cBoxes[t + 1         ].corners, 4, b);//2
        setBitAt(ref arr_cBoxes[t + 1 + q     ].corners, 7, b);//1
        setBitAt(ref arr_cBoxes[t +     q     ].corners, 6, b);//0
    }

    void randomizeGridBox()
    {
        for (int y = 0; y < (dimY-1); y++)
        {
            for (int z = 0; z < (dimZ-1); z++)
            {
                for (int x = 0; x < (dimX-1); x++)
                {
                    Destroy(arr_dummyGridBoxes[y * (dimX - 1) * (dimZ - 1) + z * (dimZ - 1) + x]);

                    if (Random.value < 0.5f)
                    {
                        GameObject cInst = Instantiate(pf_dummyGridBox.gameObject, new Vector3(x + 0.5f, y + 0.5f, z + 0.5f), Quaternion.identity, this.transform);
                        arr_dummyGridBoxes[y * (dimX - 1) * (dimZ - 1) + z * (dimZ - 1) + x] = cInst;
                        cInst.name = "D_" + x + "x" + y + "y" + z + "z";

                        enableGridBoxCorners(x, y, z);
                    }
                    else
                        enableGridBoxCorners(x, y, z, 0);                    
                }
            }
        }
    }
    
    void SetMesh(CornerBox c, int x, int y, int z)
    {
        byte b = c.corners;

        byte bitmask    = arr_contra[b].config;
        byte rep        = arr_contra[b].Ry;

        Mesh result;
        
        if (iMeshes.TryGetValue("X_" + bitmask.ToString(), out result))
        {
            c.gameObject.GetComponent<MeshFilter>().mesh = result;
            for(int r = 0; r < rep ; r++)
            {
                c.gameObject.transform.Rotate(0,-90.0f,0, Space.Self);
            }
            c.gameObject.transform.Rotate(0,180.0f,0, Space.Self);            
        }
    }

#endregion

#region Debug

#if _Debug
    void drawWCC(CBox b, byte i, Vector3 pos)
    {
        if (bitAt(b.corners, i) > 0)
        {
            Gizmos.DrawWireCube(pos, new Vector3(0.5f, 0.5f, 0.5f));
        }
/*        else
        {
            Gizmos.color = Color.black;
            Gizmos.DrawWireCube(pos, new Vector3(0.1f, 0.1f, 0.1f));
        }
*/
    }

    private void OnDrawGizmos()
    {
        if (Application.isPlaying) {
            for (int y = 0; y < dimY; y++)
            {
                for (int z = 0; z < dimZ; z++)
                {
                    for (int x = 0; x < dimX; x++)
                    {
                        CBox b = arr_cBoxes[y * dimX * dimZ + z * dimZ + x];

                        Gizmos.color = Color.black;
                        Gizmos.DrawWireCube(new Vector3(x,y,z), new Vector3(1, 1, 1));

                        Gizmos.color = Color.red;
                        drawWCC(b, 7, new Vector3(x - .25f, y + .25f, z - .25f));
                        drawWCC(b, 6, new Vector3(x + .25f, y + .25f, z - .25f));
                        drawWCC(b, 5, new Vector3(x + .25f, y + .25f, z + .25f));
                        drawWCC(b, 4, new Vector3(x - .25f, y + .25f, z + .25f));

                        drawWCC(b, 3, new Vector3(x - .25f, y - .25f, z - .25f));
                        drawWCC(b, 2, new Vector3(x + .25f, y - .25f, z - .25f));
                        drawWCC(b, 1, new Vector3(x + .25f, y - .25f, z + .25f));
                        drawWCC(b, 0, new Vector3(x - .25f, y - .25f, z + .25f));
                    }
                }
            }
        }
    }
#endif
    System.Collections.IEnumerator WaitForKeyDown()
    {
//        bool stop = false;
        int x = 0, y = 0, z = 0;
        while (!S_Input.MiscKeyMap.Escape.triggered/* && !stop*/)
        {
            if(S_Input.MiscKeyMap.Space.triggered){

                randomizeGridBox();

                fillCornerObjects();

                x++;
                if(x == (dimX-1))
                {
                    x = 0;
                    z++;
                    if(z == (dimZ-1))
                    {
                        z = 0;
                        y++;
                        if(y == (dimY-1))
                        {
//                            stop = true;
                        }
                    }
                }
            }
            yield return null;
        }
        yield return new WaitForSeconds(1f);
        Debug.Log("Done");
    }
#endregion

#region BitFiddling

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

    byte setByte(byte x, byte i7, byte i6, byte i5, byte i4, byte i3, byte i2, byte i1, byte i0)
    {
        return (byte)(
            (bitAt(x, i7) << 7) + 
            (bitAt(x, i6) << 6) +
            (bitAt(x, i5) << 5) +
            (bitAt(x, i4) << 4) +
            (bitAt(x, i3) << 3) +
            (bitAt(x, i2) << 2) +
            (bitAt(x, i1) << 1) + 
            (bitAt(x, i0)));
    }

    byte Ry(byte x)
    {
        return setByte(x, 4,7,6,5,0,3,2,1);
    }

    void printByte(byte b)
    {
        Debug.Log(b + " " + (b >> 7 & 1) + "" + (b >> 6 & 1) + ""  + (b >> 5 & 1) + ""  + (b >> 4 & 1) + ""  + 
                            (b >> 3 & 1) + "" + (b >> 2 & 1) + ""  + (b >> 1 & 1) + ""  + (b >> 0 & 1));
    }

    byte contraFormulae(contra c)
    {
        byte res = c.config;

        for(int i = 0; i < (c.Ry & 15); i++)
        {
            res = Ry(res);
        }
        return res;
    }

    void rAppendContra(contra[] contraArr, byte config, byte repitationY)
    {
        for (byte i = 0; i < repitationY; i++)
        {
            contra b = new contra(config, i, 0);
            byte h = contraFormulae(b);

            contraArr[h] = b;
        }
    }
    #endregion

#region Misc
    Vector3Int stringxyzExtract(string a)
    {
        string s = string.Empty;
        Vector3Int xyz = Vector3Int.zero;

        for (int i=0; i< a.Length; i++)
        {
            if (System.Char.IsDigit(a[i]))
            {
                s += a[i];
            }
            if (a[i] == 'x')
            {
                xyz.x = int.Parse(s);
                s = string.Empty;
            }
            if (a[i] == 'y')
            {
                xyz.y = int.Parse(s);
                s = string.Empty;
            }
            if (a[i] == 'z')
            {
                xyz.z = int.Parse(s);
                s = string.Empty;
            }
        }
        return xyz;
    }

#endregion
}
