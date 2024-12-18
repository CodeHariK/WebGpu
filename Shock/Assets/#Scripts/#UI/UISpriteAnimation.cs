using UnityEngine;
using UnityEngine.UI;

public class UISpriteAnimation : MonoBehaviour
{
	[SerializeField] float TimePerSprite = 0.1f;
	[SerializeField] bool Loop           = false;
	[SerializeField] string Path         = "";

	Image _Image            = null;
	Sprite[] _Sprites       = null;
	float _ElapsedTime      = 0f;
	int _CurrentSpritenum   = 0;

	void Start()
	{
		_Image = GetComponent<Image>();
		enabled = false;
		_Sprites = Resources.LoadAll<Sprite>(Path);
		if (_Sprites != null && _Sprites.Length > 0)
		{
			//mTimePerFrame = 1f / _FrameRate;
			enabled = true;
		}
		else
		{
			Debug.LogError("Failed to load Sprite Sheet " + _Sprites.Length);
		}
	}

	void Update()
	{
		_ElapsedTime += Time.deltaTime;		
		if (_ElapsedTime >= TimePerSprite)
		{
			_ElapsedTime = 0f;
			++_CurrentSpritenum;
			if (_CurrentSpritenum >= _Sprites.Length)
			{
				if (Loop)
					_CurrentSpritenum = 0;
				else 
					enabled = false;
			}
			if(_CurrentSpritenum >= 0 && _CurrentSpritenum < _Sprites.Length)
			{
				_Image.sprite = _Sprites[_CurrentSpritenum];
			}
		}
	}
}
