using UnityEngine;

public class ButtonSquish : MonoBehaviour
{
    Animation ani;

    void Start()
    {
        ani = GetComponent<Animation>();
    }

    public void Button_touch()
	{
        ani.Play("ButtonSquishandStretch");
	}
}
