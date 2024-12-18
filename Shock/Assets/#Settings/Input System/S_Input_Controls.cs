// GENERATED AUTOMATICALLY FROM 'Assets/#Settings/Input System/S_Input_Controls.inputactions'

using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.Utilities;

public class @S_Input_Controls : IInputActionCollection, IDisposable
{
    public InputActionAsset asset { get; }
    public @S_Input_Controls()
    {
        asset = InputActionAsset.FromJson(@"{
    ""name"": ""S_Input_Controls"",
    ""maps"": [
        {
            ""name"": ""PlayerKeyMap"",
            ""id"": ""0f9d488a-0635-44be-9b86-a686cc87618c"",
            ""actions"": [
                {
                    ""name"": ""Move"",
                    ""type"": ""Value"",
                    ""id"": ""44511721-3d1f-470c-84a1-1c8ee227bef7"",
                    ""expectedControlType"": ""Vector2"",
                    ""processors"": """",
                    ""interactions"": """"
                },
                {
                    ""name"": ""Jump"",
                    ""type"": ""Button"",
                    ""id"": ""671a1dd6-18d2-4307-aaa1-3b89c76bcb66"",
                    ""expectedControlType"": ""Button"",
                    ""processors"": """",
                    ""interactions"": """"
                },
                {
                    ""name"": ""Look"",
                    ""type"": ""Value"",
                    ""id"": ""57b99943-8032-4685-a478-1d4a1357b3f1"",
                    ""expectedControlType"": ""Vector2"",
                    ""processors"": """",
                    ""interactions"": """"
                }
            ],
            ""bindings"": [
                {
                    ""name"": """",
                    ""id"": ""8a4d7166-1f26-4a8a-b8c9-a753300aaf56"",
                    ""path"": ""<Gamepad>/leftStick"",
                    ""interactions"": """",
                    ""processors"": ""InvertVector2(invertX=false)"",
                    ""groups"": """",
                    ""action"": ""Move"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                },
                {
                    ""name"": ""WASD"",
                    ""id"": ""756db4b1-9c93-4e7e-a441-44ef65e7f97a"",
                    ""path"": ""2DVector"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""Move"",
                    ""isComposite"": true,
                    ""isPartOfComposite"": false
                },
                {
                    ""name"": ""up"",
                    ""id"": ""de2d49f1-d6bf-44b0-9309-004c1f5a2a94"",
                    ""path"": ""<Keyboard>/w"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""Move"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": true
                },
                {
                    ""name"": ""down"",
                    ""id"": ""e5f80570-e6c8-418b-9e60-899cb604caff"",
                    ""path"": ""<Keyboard>/s"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""Move"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": true
                },
                {
                    ""name"": ""left"",
                    ""id"": ""59cc5b35-b9e6-420e-a3b3-d99a6e8cb8e1"",
                    ""path"": ""<Keyboard>/a"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""Move"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": true
                },
                {
                    ""name"": ""right"",
                    ""id"": ""6fe014e3-39ec-44a5-9ccc-23ea873db776"",
                    ""path"": ""<Keyboard>/d"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""Move"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": true
                },
                {
                    ""name"": """",
                    ""id"": ""dbdee6ca-7257-4c95-9c3b-8180d34d277f"",
                    ""path"": ""<Keyboard>/space"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""Jump"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                },
                {
                    ""name"": """",
                    ""id"": ""8841c7fd-5b9d-4fcc-9af2-49ba8a6b999a"",
                    ""path"": ""<Gamepad>/rightStick"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""Look"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                },
                {
                    ""name"": """",
                    ""id"": ""0a37c33e-eddf-485e-920e-297116822664"",
                    ""path"": ""<Mouse>/delta"",
                    ""interactions"": """",
                    ""processors"": ""InvertVector2(invertX=false)"",
                    ""groups"": """",
                    ""action"": ""Look"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                }
            ]
        },
        {
            ""name"": ""MiscKeyMap"",
            ""id"": ""4a1cbcec-4d3f-4fdf-87b4-a0595d9d4c61"",
            ""actions"": [
                {
                    ""name"": ""Escape"",
                    ""type"": ""Button"",
                    ""id"": ""970fa179-a43b-4c7e-aa43-6f92c4a92c82"",
                    ""expectedControlType"": ""Button"",
                    ""processors"": """",
                    ""interactions"": """"
                },
                {
                    ""name"": ""C"",
                    ""type"": ""Button"",
                    ""id"": ""e099d8b6-5ffa-44a5-aa86-1aee97ef56fd"",
                    ""expectedControlType"": ""Button"",
                    ""processors"": """",
                    ""interactions"": """"
                },
                {
                    ""name"": ""Space"",
                    ""type"": ""Button"",
                    ""id"": ""8e4ae5bf-7ad7-43ff-b586-c1b2a923ec1b"",
                    ""expectedControlType"": ""Button"",
                    ""processors"": """",
                    ""interactions"": """"
                },
                {
                    ""name"": ""LMouseClick"",
                    ""type"": ""Button"",
                    ""id"": ""9a39630d-305e-4d87-b6a2-c2533d3135b6"",
                    ""expectedControlType"": ""Button"",
                    ""processors"": """",
                    ""interactions"": """"
                },
                {
                    ""name"": ""RMouseClick"",
                    ""type"": ""Button"",
                    ""id"": ""7bc16767-78fc-4a6c-8a15-9464f7c278e8"",
                    ""expectedControlType"": ""Button"",
                    ""processors"": """",
                    ""interactions"": """"
                },
                {
                    ""name"": ""MouseLoc"",
                    ""type"": ""Value"",
                    ""id"": ""153163ab-7da7-43ba-ac85-2d4af9efa3e4"",
                    ""expectedControlType"": ""Vector2"",
                    ""processors"": """",
                    ""interactions"": """"
                }
            ],
            ""bindings"": [
                {
                    ""name"": """",
                    ""id"": ""dcb84fd2-fc65-4554-bf29-8c900e4d2e88"",
                    ""path"": ""<Keyboard>/escape"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""Escape"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                },
                {
                    ""name"": """",
                    ""id"": ""d826d7fb-8695-4d67-8f2c-190db4e2ec01"",
                    ""path"": ""<Keyboard>/c"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""C"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                },
                {
                    ""name"": """",
                    ""id"": ""d156bce0-30bd-45dc-bced-5d20daa907aa"",
                    ""path"": ""<Keyboard>/space"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""Space"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                },
                {
                    ""name"": """",
                    ""id"": ""6855edaa-d43d-41b8-b682-284b5f24e859"",
                    ""path"": ""<Mouse>/leftButton"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""LMouseClick"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                },
                {
                    ""name"": """",
                    ""id"": ""16c5c9f8-20b0-4ebc-832e-26c17178ac32"",
                    ""path"": ""<Mouse>/rightButton"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""RMouseClick"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                },
                {
                    ""name"": """",
                    ""id"": ""ae57ebdc-3631-41ff-9a8f-63d02694b3ba"",
                    ""path"": ""<Mouse>/position"",
                    ""interactions"": """",
                    ""processors"": """",
                    ""groups"": """",
                    ""action"": ""MouseLoc"",
                    ""isComposite"": false,
                    ""isPartOfComposite"": false
                }
            ]
        }
    ],
    ""controlSchemes"": []
}");
        // PlayerKeyMap
        m_PlayerKeyMap = asset.FindActionMap("PlayerKeyMap", throwIfNotFound: true);
        m_PlayerKeyMap_Move = m_PlayerKeyMap.FindAction("Move", throwIfNotFound: true);
        m_PlayerKeyMap_Jump = m_PlayerKeyMap.FindAction("Jump", throwIfNotFound: true);
        m_PlayerKeyMap_Look = m_PlayerKeyMap.FindAction("Look", throwIfNotFound: true);
        // MiscKeyMap
        m_MiscKeyMap = asset.FindActionMap("MiscKeyMap", throwIfNotFound: true);
        m_MiscKeyMap_Escape = m_MiscKeyMap.FindAction("Escape", throwIfNotFound: true);
        m_MiscKeyMap_C = m_MiscKeyMap.FindAction("C", throwIfNotFound: true);
        m_MiscKeyMap_Space = m_MiscKeyMap.FindAction("Space", throwIfNotFound: true);
        m_MiscKeyMap_LMouseClick = m_MiscKeyMap.FindAction("LMouseClick", throwIfNotFound: true);
        m_MiscKeyMap_RMouseClick = m_MiscKeyMap.FindAction("RMouseClick", throwIfNotFound: true);
        m_MiscKeyMap_MouseLoc = m_MiscKeyMap.FindAction("MouseLoc", throwIfNotFound: true);
    }

    public void Dispose()
    {
        UnityEngine.Object.Destroy(asset);
    }

    public InputBinding? bindingMask
    {
        get => asset.bindingMask;
        set => asset.bindingMask = value;
    }

    public ReadOnlyArray<InputDevice>? devices
    {
        get => asset.devices;
        set => asset.devices = value;
    }

    public ReadOnlyArray<InputControlScheme> controlSchemes => asset.controlSchemes;

    public bool Contains(InputAction action)
    {
        return asset.Contains(action);
    }

    public IEnumerator<InputAction> GetEnumerator()
    {
        return asset.GetEnumerator();
    }

    IEnumerator IEnumerable.GetEnumerator()
    {
        return GetEnumerator();
    }

    public void Enable()
    {
        asset.Enable();
    }

    public void Disable()
    {
        asset.Disable();
    }

    // PlayerKeyMap
    private readonly InputActionMap m_PlayerKeyMap;
    private IPlayerKeyMapActions m_PlayerKeyMapActionsCallbackInterface;
    private readonly InputAction m_PlayerKeyMap_Move;
    private readonly InputAction m_PlayerKeyMap_Jump;
    private readonly InputAction m_PlayerKeyMap_Look;
    public struct PlayerKeyMapActions
    {
        private @S_Input_Controls m_Wrapper;
        public PlayerKeyMapActions(@S_Input_Controls wrapper) { m_Wrapper = wrapper; }
        public InputAction @Move => m_Wrapper.m_PlayerKeyMap_Move;
        public InputAction @Jump => m_Wrapper.m_PlayerKeyMap_Jump;
        public InputAction @Look => m_Wrapper.m_PlayerKeyMap_Look;
        public InputActionMap Get() { return m_Wrapper.m_PlayerKeyMap; }
        public void Enable() { Get().Enable(); }
        public void Disable() { Get().Disable(); }
        public bool enabled => Get().enabled;
        public static implicit operator InputActionMap(PlayerKeyMapActions set) { return set.Get(); }
        public void SetCallbacks(IPlayerKeyMapActions instance)
        {
            if (m_Wrapper.m_PlayerKeyMapActionsCallbackInterface != null)
            {
                @Move.started -= m_Wrapper.m_PlayerKeyMapActionsCallbackInterface.OnMove;
                @Move.performed -= m_Wrapper.m_PlayerKeyMapActionsCallbackInterface.OnMove;
                @Move.canceled -= m_Wrapper.m_PlayerKeyMapActionsCallbackInterface.OnMove;
                @Jump.started -= m_Wrapper.m_PlayerKeyMapActionsCallbackInterface.OnJump;
                @Jump.performed -= m_Wrapper.m_PlayerKeyMapActionsCallbackInterface.OnJump;
                @Jump.canceled -= m_Wrapper.m_PlayerKeyMapActionsCallbackInterface.OnJump;
                @Look.started -= m_Wrapper.m_PlayerKeyMapActionsCallbackInterface.OnLook;
                @Look.performed -= m_Wrapper.m_PlayerKeyMapActionsCallbackInterface.OnLook;
                @Look.canceled -= m_Wrapper.m_PlayerKeyMapActionsCallbackInterface.OnLook;
            }
            m_Wrapper.m_PlayerKeyMapActionsCallbackInterface = instance;
            if (instance != null)
            {
                @Move.started += instance.OnMove;
                @Move.performed += instance.OnMove;
                @Move.canceled += instance.OnMove;
                @Jump.started += instance.OnJump;
                @Jump.performed += instance.OnJump;
                @Jump.canceled += instance.OnJump;
                @Look.started += instance.OnLook;
                @Look.performed += instance.OnLook;
                @Look.canceled += instance.OnLook;
            }
        }
    }
    public PlayerKeyMapActions @PlayerKeyMap => new PlayerKeyMapActions(this);

    // MiscKeyMap
    private readonly InputActionMap m_MiscKeyMap;
    private IMiscKeyMapActions m_MiscKeyMapActionsCallbackInterface;
    private readonly InputAction m_MiscKeyMap_Escape;
    private readonly InputAction m_MiscKeyMap_C;
    private readonly InputAction m_MiscKeyMap_Space;
    private readonly InputAction m_MiscKeyMap_LMouseClick;
    private readonly InputAction m_MiscKeyMap_RMouseClick;
    private readonly InputAction m_MiscKeyMap_MouseLoc;
    public struct MiscKeyMapActions
    {
        private @S_Input_Controls m_Wrapper;
        public MiscKeyMapActions(@S_Input_Controls wrapper) { m_Wrapper = wrapper; }
        public InputAction @Escape => m_Wrapper.m_MiscKeyMap_Escape;
        public InputAction @C => m_Wrapper.m_MiscKeyMap_C;
        public InputAction @Space => m_Wrapper.m_MiscKeyMap_Space;
        public InputAction @LMouseClick => m_Wrapper.m_MiscKeyMap_LMouseClick;
        public InputAction @RMouseClick => m_Wrapper.m_MiscKeyMap_RMouseClick;
        public InputAction @MouseLoc => m_Wrapper.m_MiscKeyMap_MouseLoc;
        public InputActionMap Get() { return m_Wrapper.m_MiscKeyMap; }
        public void Enable() { Get().Enable(); }
        public void Disable() { Get().Disable(); }
        public bool enabled => Get().enabled;
        public static implicit operator InputActionMap(MiscKeyMapActions set) { return set.Get(); }
        public void SetCallbacks(IMiscKeyMapActions instance)
        {
            if (m_Wrapper.m_MiscKeyMapActionsCallbackInterface != null)
            {
                @Escape.started -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnEscape;
                @Escape.performed -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnEscape;
                @Escape.canceled -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnEscape;
                @C.started -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnC;
                @C.performed -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnC;
                @C.canceled -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnC;
                @Space.started -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnSpace;
                @Space.performed -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnSpace;
                @Space.canceled -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnSpace;
                @LMouseClick.started -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnLMouseClick;
                @LMouseClick.performed -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnLMouseClick;
                @LMouseClick.canceled -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnLMouseClick;
                @RMouseClick.started -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnRMouseClick;
                @RMouseClick.performed -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnRMouseClick;
                @RMouseClick.canceled -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnRMouseClick;
                @MouseLoc.started -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnMouseLoc;
                @MouseLoc.performed -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnMouseLoc;
                @MouseLoc.canceled -= m_Wrapper.m_MiscKeyMapActionsCallbackInterface.OnMouseLoc;
            }
            m_Wrapper.m_MiscKeyMapActionsCallbackInterface = instance;
            if (instance != null)
            {
                @Escape.started += instance.OnEscape;
                @Escape.performed += instance.OnEscape;
                @Escape.canceled += instance.OnEscape;
                @C.started += instance.OnC;
                @C.performed += instance.OnC;
                @C.canceled += instance.OnC;
                @Space.started += instance.OnSpace;
                @Space.performed += instance.OnSpace;
                @Space.canceled += instance.OnSpace;
                @LMouseClick.started += instance.OnLMouseClick;
                @LMouseClick.performed += instance.OnLMouseClick;
                @LMouseClick.canceled += instance.OnLMouseClick;
                @RMouseClick.started += instance.OnRMouseClick;
                @RMouseClick.performed += instance.OnRMouseClick;
                @RMouseClick.canceled += instance.OnRMouseClick;
                @MouseLoc.started += instance.OnMouseLoc;
                @MouseLoc.performed += instance.OnMouseLoc;
                @MouseLoc.canceled += instance.OnMouseLoc;
            }
        }
    }
    public MiscKeyMapActions @MiscKeyMap => new MiscKeyMapActions(this);
    public interface IPlayerKeyMapActions
    {
        void OnMove(InputAction.CallbackContext context);
        void OnJump(InputAction.CallbackContext context);
        void OnLook(InputAction.CallbackContext context);
    }
    public interface IMiscKeyMapActions
    {
        void OnEscape(InputAction.CallbackContext context);
        void OnC(InputAction.CallbackContext context);
        void OnSpace(InputAction.CallbackContext context);
        void OnLMouseClick(InputAction.CallbackContext context);
        void OnRMouseClick(InputAction.CallbackContext context);
        void OnMouseLoc(InputAction.CallbackContext context);
    }
}
